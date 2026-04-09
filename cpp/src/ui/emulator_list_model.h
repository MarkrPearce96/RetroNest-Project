#pragma once

#include "core/manifest_loader.h"
#include <QObject>
#include <QVariantList>
#include <QStringList>
#include <QMap>

class EmulatorListModel : public QObject {
    Q_OBJECT

public:
    explicit EmulatorListModel(ManifestLoader* loader, QObject* parent = nullptr);

    Q_INVOKABLE QVariantList allEmulators() const;
    Q_INVOKABLE void toggleEmulator(const QString& id);
    Q_INVOKABLE QStringList selectedEmulatorIds() const;

    Q_INVOKABLE QVariantList resolutionOptions(const QString& emuId) const;
    Q_INVOKABLE QVariantList aspectRatioOptions(const QString& emuId) const;
    Q_INVOKABLE QString defaultResolution(const QString& emuId) const;
    Q_INVOKABLE QString defaultAspectRatio(const QString& emuId) const;
    Q_INVOKABLE void setResolution(const QString& emuId, const QString& value);
    Q_INVOKABLE void setAspectRatio(const QString& emuId, const QString& label);
    Q_INVOKABLE QString chosenResolution(const QString& emuId) const;
    Q_INVOKABLE QString chosenAspectRatio(const QString& emuId) const;

    Q_INVOKABLE QVariantList biosStatus(const QString& emuId) const;
    Q_INVOKABLE QStringList availableSystems() const;

    // Accessors for InstallController
    ManifestLoader* loader() const { return m_loader; }
    QMap<QString, QString> resolutionChoices() const { return m_resolutionChoices; }
    QMap<QString, QString> aspectRatioChoices() const { return m_aspectRatioChoices; }

signals:
    void selectedEmulatorsChanged();

private:
    ManifestLoader* m_loader;
    QMap<QString, bool> m_selected;
    QMap<QString, QString> m_resolutionChoices;
    QMap<QString, QString> m_aspectRatioChoices;
};
