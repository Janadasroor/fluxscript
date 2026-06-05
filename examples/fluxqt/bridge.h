#ifndef FLUXQT_BRIDGE_H
#define FLUXQT_BRIDGE_H

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QOpenGLWidget>
#include <QTimer>
#include <mutex>

namespace Flux { class JITEngine; }
class QMainWindow;

extern void callFluxFunction(const char* name);

class FluxQtOpenGLWidget : public QOpenGLWidget {
    Q_OBJECT
public:
    void setInitCallback(const char* name)  { m_init  = QString::fromUtf8(name); }
    void setPaintCallback(const char* name) { m_paint = QString::fromUtf8(name); }
    void setResizeCallback(const char* name){ m_resize= QString::fromUtf8(name); }
    void startAutoUpdate(int fps) {
        if (!m_timer) {
            m_timer = new QTimer(this);
            connect(m_timer, &QTimer::timeout, this, [this]{ update(); });
        }
        m_timer->start(fps > 0 ? 1000 / fps : 0);
    }
    void stopAutoUpdate() { if (m_timer) m_timer->stop(); }

protected:
    void initializeGL() override {
        if (!m_init.isEmpty()) callFluxFunction(m_init.toUtf8().constData());
    }
    void paintGL() override {
        if (!m_paint.isEmpty()) callFluxFunction(m_paint.toUtf8().constData());
    }
    void resizeGL(int, int) override {
        if (!m_resize.isEmpty()) callFluxFunction(m_resize.toUtf8().constData());
    }

private:
    QString m_init, m_paint, m_resize;
    QTimer* m_timer = nullptr;
};

class FluxQtBridge : public QObject {
    Q_OBJECT
public:
    static FluxQtBridge& instance();
    double registerObject(QObject* obj);
    QObject* resolveHandle(double handle, const char* caller = nullptr);
    void connectSignalByName(double handle, const char* signal, const char* funcName);
    void clearRegistry();
    void setPersistentWindow(QMainWindow* win);
    QMainWindow* persistentWindow() const { return m_persistentWindow; }

public slots:
    void onBridgeEvent();

private:
    explicit FluxQtBridge(QObject* parent = nullptr);
    mutable std::mutex m_mutex;
    QHash<void*, QPointer<QObject>> m_registry;
    QHash<QObject*, QString> m_signalNameMap;
    QMainWindow* m_persistentWindow = nullptr;
};

void registerFluxQtSymbols(Flux::JITEngine& jit);

#endif
