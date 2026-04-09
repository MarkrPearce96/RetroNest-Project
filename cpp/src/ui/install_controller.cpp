#include "install_controller.h"
#include "adapters/adapter_registry.h"
#include "core/paths.h"
#include "core/ini_file.h"
#include "core/database.h"
#include "services/emulator_service.h"
#include "services/game_service.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QDebug>

InstallController::InstallController(EmulatorListModel* model, QObject* parent)
    : QObject(parent), m_model(model)
{
}

bool InstallController::installing() const { return m_installing; }
int InstallController::installCurrent() const { return m_installCurrent; }
int InstallController::installTotal() const { return m_installTotal; }
QString InstallController::installStatus() const { return m_installStatus; }
bool InstallController::installDone() const { return m_installDone; }

double InstallController::progress() const {
    if (m_installTotal == 0) return 0.0;
    return static_cast<double>(m_installCurrent) / m_installTotal;
}

void InstallController::startInstall(const QString& rootPath) {
    m_installing = true;
    emit installingChanged();

    Paths::saveRoot(rootPath);
    Paths::setRoot(rootPath);
    Paths::ensureDirectories();

    QStringList selected = m_model->selectedEmulatorIds();
    m_installTotal = selected.size();
    emit installTotalChanged();

    ManifestLoader* loader = m_model->loader();
    EmulatorService emuService(loader);

    for (int i = 0; i < selected.size(); ++i) {
        const QString& emuId = selected[i];
        m_installCurrent = i + 1;
        emit installCurrentChanged();
        emit progressChanged();

        const auto* manifest = loader->emulatorById(emuId);
        QString name = manifest ? manifest->name : emuId;

        m_installStatus = QString("Installing %1... (%2/%3)").arg(name).arg(m_installCurrent).arg(m_installTotal);
        emit installStatusChanged();
        QApplication::processEvents();

        auto result = emuService.installEmulatorSync(emuId);
        if (!result.success)
            qWarning() << "Install failed for" << emuId << ":" << result.message;
    }

    // Apply resolution + aspect ratio settings
    m_installStatus = "Applying settings...";
    emit installStatusChanged();
    QApplication::processEvents();

    const auto resChoices = m_model->resolutionChoices();
    const auto arChoices = m_model->aspectRatioChoices();

    for (const auto& emuId : selected) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) continue;

        const QString configPath = adapter->configFilePath();
        if (configPath.isEmpty()) continue;

        IniFile ini;
        ini.load(configPath);

        const auto resOpts = adapter->resolutionOptions();
        if (!resOpts.options.isEmpty() && resChoices.contains(emuId)) {
            const QString chosen = resChoices[emuId];
            ini.setValue(resOpts.section, resOpts.key, chosen);
            qInfo() << "[Wizard] Set" << resOpts.section << "/" << resOpts.key
                    << "=" << chosen << "for" << emuId;
        }

        const auto arOpts = adapter->aspectRatioOptions();
        if (!arOpts.options.isEmpty() && arChoices.contains(emuId)) {
            const QString chosenLabel = arChoices[emuId];
            for (const auto& opt : arOpts.options) {
                if (opt.label != chosenLabel) continue;
                for (const auto& patch : opt.patches) {
                    ini.setValue(patch.section, patch.key, patch.value);
                    qInfo() << "[Wizard] Set" << patch.section << "/" << patch.key
                            << "=" << patch.value << "for" << emuId;
                }
                break;
            }
        }

        ini.save(configPath);
    }

    // Ensure per-system ROM directories exist
    {
        QSet<QString> systemSet;
        for (const auto& emuId : selected) {
            const auto* manifest = loader->emulatorById(emuId);
            if (!manifest) continue;
            for (const auto& sys : manifest->systems)
                systemSet.insert(sys);
        }
        Paths::ensureRomDirectories(systemSet.values());
    }

    // Scan per-system ROM folders for any ROMs added during wizard
    {
        m_installStatus = "Scanning ROM folders...";
        emit installStatusChanged();
        QApplication::processEvents();

        Database db;
        QString dbPath = Paths::configDir() + "/retronest.db";
        QDir().mkpath(QFileInfo(dbPath).absolutePath());
        if (db.open(dbPath)) {
            GameService gameService(loader, &db);
            auto importResult = gameService.scanRomFolders();
            qInfo() << "[Wizard] ROM scan:" << importResult.message;
            db.close();
        }
    }

    m_installStatus = QString("Setup complete! Installed %1 emulator(s).").arg(selected.size());
    emit installStatusChanged();

    m_installing = false;
    emit installingChanged();

    m_installDone = true;
    emit installDoneChanged();
    emit progressChanged();
}
