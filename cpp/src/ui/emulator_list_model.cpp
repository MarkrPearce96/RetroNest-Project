#include "emulator_list_model.h"
#include "adapters/adapter_registry.h"
#include "core/paths.h"

#include <QVariantMap>
#include <QFileInfo>
#include <QSet>

EmulatorListModel::EmulatorListModel(ManifestLoader* loader, QObject* parent)
    : QObject(parent), m_loader(loader)
{
    // Pre-select all emulators and set defaults
    for (const auto& emu : m_loader->allEmulators()) {
        m_selected[emu.id] = true;

        auto* adapter = AdapterRegistry::instance().adapterFor(emu.id);
        if (adapter) {
            auto resOpts = adapter->resolutionOptions();
            if (!resOpts.options.isEmpty())
                m_resolutionChoices[emu.id] = resOpts.defaultValue;
            auto arOpts = adapter->aspectRatioOptions();
            if (!arOpts.options.isEmpty())
                m_aspectRatioChoices[emu.id] = arOpts.defaultLabel;
        }
    }
}

QVariantList EmulatorListModel::allEmulators() const {
    QVariantList list;
    for (const auto& emu : m_loader->allEmulators()) {
        QVariantMap item;
        item["id"] = emu.id;
        item["name"] = emu.name;
        item["systems"] = emu.systems.join(", ");
        item["logo"] = emu.logo;
        item["selected"] = m_selected.value(emu.id, false);
        list.append(item);
    }
    return list;
}

void EmulatorListModel::toggleEmulator(const QString& id) {
    m_selected[id] = !m_selected.value(id, false);
    emit selectedEmulatorsChanged();
}

QStringList EmulatorListModel::selectedEmulatorIds() const {
    QStringList ids;
    for (auto it = m_selected.constBegin(); it != m_selected.constEnd(); ++it) {
        if (it.value()) ids.append(it.key());
    }
    return ids;
}

QVariantList EmulatorListModel::resolutionOptions(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    auto opts = adapter->resolutionOptions();
    for (const auto& opt : opts.options) {
        QVariantMap item;
        item["label"] = opt.label;
        item["value"] = opt.value;
        list.append(item);
    }
    return list;
}

QVariantList EmulatorListModel::aspectRatioOptions(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    auto opts = adapter->aspectRatioOptions();
    for (const auto& opt : opts.options) {
        QVariantMap item;
        item["label"] = opt.label;
        list.append(item);
    }
    return list;
}

QString EmulatorListModel::defaultResolution(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->resolutionOptions().defaultValue;
}

QString EmulatorListModel::defaultAspectRatio(const QString& emuId) const {
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return {};
    return adapter->aspectRatioOptions().defaultLabel;
}

void EmulatorListModel::setResolution(const QString& emuId, const QString& value) {
    m_resolutionChoices[emuId] = value;
}

void EmulatorListModel::setAspectRatio(const QString& emuId, const QString& label) {
    m_aspectRatioChoices[emuId] = label;
}

QString EmulatorListModel::chosenResolution(const QString& emuId) const {
    return m_resolutionChoices.value(emuId);
}

QString EmulatorListModel::chosenAspectRatio(const QString& emuId) const {
    return m_aspectRatioChoices.value(emuId);
}

QVariantList EmulatorListModel::biosStatus(const QString& emuId) const {
    QVariantList list;
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (!adapter) return list;

    QString biosDir = Paths::biosDir();
    auto biosList = adapter->biosFiles();

    for (const auto& bios : biosList) {
        QVariantMap item;
        item["filename"] = bios.filename;
        item["description"] = bios.description;
        item["required"] = bios.required;
        item["found"] = QFileInfo::exists(biosDir + "/" + bios.filename);
        list.append(item);
    }
    return list;
}

QStringList EmulatorListModel::availableSystems() const {
    QSet<QString> systems;
    for (const auto& emu : m_loader->allEmulators()) {
        if (!m_selected.value(emu.id, false)) continue;
        for (const auto& sys : emu.systems)
            systems.insert(sys);
    }
    return systems.values();
}
