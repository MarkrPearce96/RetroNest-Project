#include "install_controller.h"
#include "adapters/adapter_registry.h"
#include "core/paths.h"
#include "core/ini_file.h"
#include "core/database.h"
#include "services/emulator_service.h"
#include "services/game_service.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QDebug>
#include <QtConcurrent>
#include <QFutureWatcher>

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
    // Runtime-only root setup; persistence happens in WizardState::accept
    // so it isn't tied to this page being visited.
    Paths::setRoot(rootPath);
    Paths::ensureDirectories();

    m_pendingEmulators = m_model->selectedEmulatorIds();
    m_pendingIndex = 0;
    m_installTotal = m_pendingEmulators.size();
    m_installCurrent = 0;
    m_installDone = false;
    m_installing = true;
    emit installTotalChanged();
    emit installCurrentChanged();
    emit progressChanged();
    emit installingChanged();
    emit installDoneChanged();

    if (!m_emuService) {
        m_emuService = std::make_unique<EmulatorService>(m_model->loader(), this);
        // Forward in-flight per-byte progress as the wizard's status line so
        // long downloads aren't a frozen "Installing X..." message. The
        // outer counter (X/Y) is driven by installFinished below.
        connect(m_emuService.get(), &EmulatorService::installProgress, this,
                [this](const QString&, double, const QString& phase, const QString& detail) {
                    if (!detail.isEmpty())
                        m_installStatus = phase + ": " + detail;
                    else
                        m_installStatus = phase + "...";
                    emit installStatusChanged();
                });
        connect(m_emuService.get(), &EmulatorService::installFinished, this,
                [this](const QString& emuId, bool ok, const QString& msg) {
                    if (!ok)
                        qWarning() << "[Wizard] Install failed for" << emuId << ":" << msg;
                    ++m_pendingIndex;
                    installNextOrFinish();
                });
    }

    installNextOrFinish();
}

void InstallController::installNextOrFinish() {
    if (m_pendingIndex >= m_pendingEmulators.size()) {
        runPostInstall();
        return;
    }

    const QString& emuId = m_pendingEmulators[m_pendingIndex];
    m_installCurrent = m_pendingIndex + 1;
    emit installCurrentChanged();
    emit progressChanged();

    const auto* manifest = m_model->loader()->emulatorById(emuId);
    QString name = manifest ? manifest->name : emuId;
    m_installStatus = QString("Installing %1... (%2/%3)")
                          .arg(name).arg(m_installCurrent).arg(m_installTotal);
    emit installStatusChanged();

    m_emuService->installEmulatorAsync(emuId);
}

void InstallController::runPostInstall() {
    ManifestLoader* loader = m_model->loader();

    // Apply resolution + aspect ratio settings (in-memory then atomic save).
    m_installStatus = "Applying settings...";
    emit installStatusChanged();

    const auto resChoices = m_model->resolutionChoices();
    const auto arChoices = m_model->aspectRatioChoices();

    for (const auto& emuId : m_pendingEmulators) {
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
        for (const auto& emuId : m_pendingEmulators) {
            const auto* manifest = loader->emulatorById(emuId);
            if (!manifest) continue;
            for (const auto& sys : manifest->systems)
                systemSet.insert(sys);
        }
        Paths::ensureRomDirectories(systemSet.values());
    }

    // ROM scan on a worker thread — large libraries can stat thousands of
    // files. The watcher captures the wizard's final message and emits the
    // done signals on the GUI thread.
    m_installStatus = "Scanning ROM folders...";
    emit installStatusChanged();

    auto* watcher = new QFutureWatcher<int>(this);
    const int totalEmulators = m_pendingEmulators.size();
    connect(watcher, &QFutureWatcher<int>::finished, this,
            [this, watcher, totalEmulators]() {
                watcher->deleteLater();

                m_installStatus = QString("Setup complete! Installed %1 emulator(s).")
                                       .arg(totalEmulators);
                emit installStatusChanged();

                m_installing = false;
                emit installingChanged();

                m_installDone = true;
                emit installDoneChanged();
                emit progressChanged();
            });

    watcher->setFuture(QtConcurrent::run([loader]() -> int {
        Database db;
        QString dbPath = Paths::configDir() + "/retronest.db";
        QDir().mkpath(QFileInfo(dbPath).absolutePath());
        if (!db.open(dbPath)) return 0;
        GameService gameService(loader, &db);
        auto importResult = gameService.scanRomFolders();
        qInfo() << "[Wizard] ROM scan:" << importResult.message;
        db.close();
        return importResult.added;
    }));
}
