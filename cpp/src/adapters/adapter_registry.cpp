#include "adapter_registry.h"
#include "pcsx2_adapter.h"
#include "duckstation_adapter.h"
#include "ppsspp_adapter.h"

#include <QDebug>

AdapterRegistry& AdapterRegistry::instance() {
    static AdapterRegistry registry;
    return registry;
}

void AdapterRegistry::registerBuiltinAdapters() {
    registerAdapter("pcsx2", std::make_unique<PCSX2Adapter>());
    registerAdapter("duckstation", std::make_unique<DuckStationAdapter>());
    registerAdapter("ppsspp", std::make_unique<PPSSPPAdapter>());
}

void AdapterRegistry::registerAdapter(const QString& id, std::unique_ptr<EmulatorAdapter> adapter) {
    m_adapters[id] = std::move(adapter);
}

EmulatorAdapter* AdapterRegistry::adapterFor(const QString& id) const {
    auto it = m_adapters.find(id);
    if (it == m_adapters.end()) {
        qWarning() << "[AdapterRegistry] No adapter registered for" << id;
        return nullptr;
    }
    return it->second.get();
}

QStringList AdapterRegistry::validateManifests(const ManifestLoader& loader) const {
    QStringList orphaned;
    for (const auto& manifest : loader.allEmulators()) {
        if (m_adapters.find(manifest.id) == m_adapters.end()) {
            qWarning() << "[AdapterRegistry] Manifest" << manifest.id
                        << "has no registered adapter — it will be non-functional";
            orphaned.append(manifest.id);
        }
    }
    return orphaned;
}
