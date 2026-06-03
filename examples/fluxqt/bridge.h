#ifndef FLUXQT_BRIDGE_H
#define FLUXQT_BRIDGE_H

#include <QObject>
#include <QPointer>
#include <QHash>
#include <mutex>

namespace Flux { class JITEngine; }

class FluxQtBridge : public QObject {
    Q_OBJECT
public:
    static FluxQtBridge& instance();
    double registerObject(QObject* obj);
    QObject* resolveHandle(double handle, const char* caller = nullptr);
    void connectSignalByName(double handle, const char* signal, const char* funcName);

public slots:
    void onBridgeEvent();

private:
    explicit FluxQtBridge(QObject* parent = nullptr);
    mutable std::mutex m_mutex;
    QHash<void*, QPointer<QObject>> m_registry;
    QHash<QObject*, QString> m_signalNameMap;
};

void registerFluxQtSymbols(Flux::JITEngine& jit);

#endif
