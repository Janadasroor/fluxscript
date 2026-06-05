// fluxqt standalone runner with hot reload
// Links the FluxScript JIT engine with the Qt widget bridge.
// Usage: fluxqt <file.flux>

#include "bridge.h"
#include <flux/jit_engine.h>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QString>
#include <QMainWindow>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <vector>

#include <llvm/Support/InitLLVM.h>

static Flux::JITEngine* g_jit = nullptr;

extern "C" double flux_print_string(double);

void callFluxFunction(const char* name) {
    if (!g_jit) return;
    std::vector<double> args;
    g_jit->callFunction(name, args);
}

static std::map<double, double> g_vars;
static std::mutex g_vars_mutex;

static void registerGlobals(Flux::JITEngine& jit) {
    jit.registerFunction("flux_set_var", (void*)(+[](double name, double val) -> double {
        std::lock_guard lock(g_vars_mutex);
        g_vars[name] = val;
        return 0.0;
    }));
    jit.registerFunction("flux_get_var", (void*)(+[](double name) -> double {
        std::lock_guard lock(g_vars_mutex);
        auto it = g_vars.find(name);
        return it != g_vars.end() ? it->second : 0.0;
    }));
    jit.registerFunction("viora_flux_print", (void*)(+[](double str) -> double {
        auto* s = reinterpret_cast<const char*>(static_cast<void*>(reinterpret_cast<int64_t*>(&str)));
        flux_print_string(str);
        return 0.0;
    }));
}

static bool compileAndRun(const std::string& source) {
    std::string error;
    if (!g_jit->compileScript(source, &error)) {
        std::cerr << "FluxScript error: " << error << "\n";
        return false;
    }
    std::string anonErr;
    g_jit->callFunction("__anon_expr", {}, &anonErr);
    if (!anonErr.empty())
        std::cerr << "[anon_expr] " << anonErr << "\n";
    return true;
}

static bool reloadScript(const std::string& filePath, QMainWindow* mw) {
    std::cerr << "\n[hot-reload] Reloading " << filePath << " ...\n";

    // Freeze painting to avoid flicker
    mw->setUpdatesEnabled(false);
    mw->setCentralWidget(new QWidget); // blank temporary placeholder

    FluxQtBridge::instance().clearRegistry(); // persistent window survives
    g_vars.clear();
    g_jit->finalize();
    g_jit->initialize();
    registerFluxQtSymbols(*g_jit);
    registerGlobals(*g_jit);

    // Re-register persistent window after JIT reset
    FluxQtBridge::instance().setPersistentWindow(mw);

    std::ifstream file(filePath);
    if (!file) { std::cerr << "[hot-reload] Cannot open file\n"; return false; }
    std::stringstream buf;
    buf << file.rdbuf();
    file.close();

    bool ok = compileAndRun(buf.str());

    // Thaw painting — new UI is fully built, paint in one frame
    mw->setUpdatesEnabled(true);
    mw->update();
    QApplication::processEvents();

    std::cerr << (ok ? "[hot-reload] Done.\n" : "[hot-reload] Failed.\n");
    return ok;
}

int main(int argc, char* argv[]) {
    llvm::InitLLVM initLLVM(argc, argv);
    QApplication app(argc, argv);

    if (argc < 2) {
        std::cerr << "Usage: fluxqt <file.flux> [file2.flux ...]\n";
        std::cerr << "  Each .flux file opens in its own window.\n";
        return 1;
    }

    // Spawn child processes for extra files beyond the first
    for (int i = 2; i < argc; i++) {
        QString childPath = QString::fromLocal8Bit(argv[i]);
        if (!QFileInfo::exists(childPath)) {
            std::cerr << "Warning: file not found: " << argv[i] << "\n";
            continue;
        }
        QString execPath = QString::fromLocal8Bit(argv[0]);
        if (!QProcess::startDetached(execPath, {childPath})) {
            std::cerr << "Warning: could not launch " << argv[i] << "\n";
        } else {
            std::cout << "Launched: " << argv[i] << "\n";
        }
    }

    std::string filePath = argv[1];

    // Change working directory to script location so relative paths work
    QDir::setCurrent(QFileInfo(QString::fromStdString(filePath)).absolutePath());

    std::ifstream file(filePath);
    if (!file) { std::cerr << "Error: could not open " << filePath << "\n"; return 1; }
    std::stringstream buf;
    buf << file.rdbuf();
    file.close();

    auto& jit = Flux::JITEngine::instance();
    g_jit = &jit;
    jit.initialize();
    registerFluxQtSymbols(jit);
    registerGlobals(jit);

    // Create persistent main window before first compile
    auto* mainWindow = new QMainWindow;
    mainWindow->resize(800, 600);
    mainWindow->show();
    FluxQtBridge::instance().setPersistentWindow(mainWindow);

    if (!compileAndRun(buf.str()))
        return 1;

    // ── Hot reload via polling (avoids file-watcher race conditions) ──
    QFileInfo fi(QString::fromStdString(filePath));
    auto* poll = new QTimer(&app);
    qint64 lastMod = fi.lastModified().toSecsSinceEpoch();

    QObject::connect(poll, &QTimer::timeout, [&]() {
        QFileInfo fi2(QString::fromStdString(filePath));
        qint64 m = fi2.lastModified().toSecsSinceEpoch();
        if (m > lastMod && fi2.size() > 0) {
            lastMod = m;
            reloadScript(filePath, mainWindow);
        }
    });
    poll->start(500);

    return app.exec();
}
