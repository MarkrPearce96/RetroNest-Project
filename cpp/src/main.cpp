#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QEventLoop>
#include <QDeadlineTimer>
#include <QRegularExpression>

#include "core/manifest_loader.h"
#include "core/paths.h"
#include "services/emulator_service.h"
#include "services/patches_installer.h"
#include "core/database.h"
#include "adapters/adapter_registry.h"
#include "ui/wizard_state.h"
#include "ui/wizard_theme.h"
#include "ui/settings_theme.h"
#include "ui/emulator_list_model.h"
#include "ui/install_controller.h"
#include "ui/game_list_model.h"
#include "ui/app_controller.h"
#include "core/game_session.h"
#include "core/libretro/core_runtime.h"
#include "ui/theme_manager.h"
#include "ui/theme_context.h"
#include "core/sdl_input_manager.h"
#include "core/macos_fullscreen.h"

#include <QCursor>
#include <QWindow>
#include <QtQml/qqmlextensionplugin.h>
#include <unistd.h>  // ::_exit — wedged-core hard exit
Q_IMPORT_QML_PLUGIN(SetupWizardPlugin)
Q_IMPORT_QML_PLUGIN(AppUIPlugin)

static int runCli(QCoreApplication& app, QCommandLineParser& parser, ManifestLoader& loader);

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("RetroNest");
    // Surfaced via QCoreApplication::applicationVersion(); RcheevosRuntime
    // composes the RA User-Agent from this so version bumps in the binary
    // are automatically reflected in HTTP requests to RA's servers. Keep
    // in sync with MACOSX_BUNDLE_VERSION in CMakeLists.txt.
    app.setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"install", "Install an emulator by manifest id", "id"});
    parser.addOption({"launch", "Launch an emulator by manifest id", "id"});
    parser.addOption({"rom", "ROM path for --launch", "path"});
    parser.addOption({"root", "Root directory for managed data", "path"});
    parser.addOption({"cli", "Run in CLI mode (no window)"});
    parser.process(app);

    // Resolve a resource directory (manifests, themes, …) relative to the
    // executable. Supports layouts for dev (bare + bundle) and installed bundle.
    // Manifests live at the project root, so from cpp/build/RetroNest that's
    // "../../" and from cpp/build/RetroNest.app/Contents/MacOS/RetroNest that's
    // "../../../../../" (up 5: MacOS, Contents, RetroNest.app, build, cpp).
    auto resolveResourceDir = [&](const QString &name) -> QString {
        const QString exeDir = app.applicationDirPath();
        const QStringList candidates = {
            exeDir + "/../../" + name,                   // dev bare exe (cpp/build/)
            exeDir + "/../../../../../" + name,          // dev .app bundle (up 5: MacOS,Contents,RetroNest.app,build,cpp)
            exeDir + "/../Resources/" + name,            // installed bundle
        };
        for (const QString &c : candidates) {
            if (QDir(c).exists()) return QDir(c).absolutePath();
        }
        return QDir(exeDir + "/../../" + name).absolutePath();
    };

    // Load manifests early (wizard needs them)
    ManifestLoader loader;
    const QString manifestsDir = resolveResourceDir("manifests");
    if (!loader.loadAll(manifestsDir)) {
        qCritical() << "Failed to load manifests from" << manifestsDir;
        return 1;
    }

    // Register adapters early (wizard needs them for biosFiles, resolutionOptions)
    AdapterRegistry::instance().registerBuiltinAdapters();

    // Validate that every manifest has a matching adapter
    auto orphaned = AdapterRegistry::instance().validateManifests(loader);
    if (!orphaned.isEmpty()) {
        qWarning() << "Manifests with no adapter (non-functional):" << orphaned.join(", ");
    }

    QQuickStyle::setStyle("Basic");

    // Determine root directory
    QString rootPath;
    if (parser.isSet("root")) {
        rootPath = parser.value("root");
    } else {
        rootPath = Paths::loadSavedRoot();
    }

    // First-run: no root configured — show QML setup wizard
    if (rootPath.isEmpty()) {
        // Context objects must be declared before the engine so they outlive it.
        // C++ destroys stack variables in reverse order — engine (last) is destroyed
        // first, preventing QML bindings from accessing deleted context objects.
        WizardTheme wizardTheme;
        WizardState wizardState;
        EmulatorListModel emulatorModel(&loader);
        InstallController installController(&emulatorModel);

        QQmlApplicationEngine engine;
        // QML modules are embedded as resources with RESOURCE_PREFIX "/", so
        // they live at qrc:/SetupWizard/ and qrc:/AppUI/. Qt's default import
        // path is qrc:/qt/qml, so we must add qrc:/ explicitly — otherwise
        // module lookup only works when launched from a CWD that happens to
        // contain the generated module dir (i.e. cpp/build/), not via
        // Launch Services (Finder double-click, `open Foo.app`).
        engine.addImportPath("qrc:/");
        engine.rootContext()->setContextProperty("WizardTheme", &wizardTheme);
        engine.rootContext()->setContextProperty("wizard", &wizardState);
        engine.rootContext()->setContextProperty("emulators", &emulatorModel);
        engine.rootContext()->setContextProperty("installer", &installController);
        engine.loadFromModule("SetupWizard", "Main");

        if (engine.rootObjects().isEmpty()) {
            qCritical() << "Failed to load QML wizard";
            return 1;
        }

        QEventLoop loop;
        QObject::connect(&wizardState, &WizardState::wizardAccepted, &loop, &QEventLoop::quit);
        QObject::connect(&engine, &QQmlApplicationEngine::quit, &loop, &QEventLoop::quit);
        loop.exec();

        rootPath = wizardState.rootPath();
        if (rootPath.isEmpty()) return 0;
    }

    // Set up paths
    if (!Paths::setRoot(rootPath)) {
        qCritical() << "Invalid root path:" << rootPath;
        return 1;
    }
    Paths::ensureDirectories();

    // Auto-fetch PCSX2 patches.zip on launch — staleness-gated, non-blocking.
    // Owned by an automatic-storage object so dtor runs at app exit (any
    // in-flight fetch is aborted via aboutToQuit, which is wired in
    // PatchesInstaller::downloadTo).
    PatchesInstaller patchesInstaller;
    {
        const QString resourcesDir = Paths::pcsx2ResourcesDir();
        QDir().mkpath(resourcesDir);

        // Connect logging before kicking off the fetch.
        QObject::connect(&patchesInstaller, &PatchesInstaller::finished, qApp,
            [](bool success, const QString& message, const QString& tag) {
                if (success) {
                    qInfo().noquote() << "[Patches]" << message
                                      << (tag.isEmpty() ? "" : "(" + tag + ")");
                } else {
                    qWarning().noquote() << "[Patches]" << message;
                }
            });
        patchesInstaller.fetchAsync(resourcesDir);
    }

    // Open database
    Database db;
    const QString dbPath = Paths::configDir() + "/retronest.db";
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    if (!db.open(dbPath)) {
        qCritical() << "Failed to open database at" << dbPath;
        return 1;
    }

    // CLI mode: --install, --launch, or --cli
    if (parser.isSet("install") || parser.isSet("launch") || parser.isSet("cli")) {
        int ret = runCli(app, parser, loader);
        db.close();

        // A wedged core's dylib stays mapped (see CoreRuntime teardown), so
        // normal exit() would run ITS static destructors under live detached
        // threads — the exact ~GSTextureCache segv this replaces. Everything
        // of ours is already flushed (db closed above); skip the destructors.
        if (CoreRuntime::anyCoreWedged()) {
            qWarning() << "[main] a core wedged this session — hard exit"
                          " (skipping process static destructors)";
            fflush(nullptr);
            ::_exit(ret);
        }
        return ret;
    }

    // GUI mode — QML app window
    {
        // Context objects declared before engine so they outlive it during destruction
        WizardTheme theme;
        AppController appController(&loader, &db);
        GameListModel gameModel(&db);
        gameModel.setMediaDir(Paths::mediaDir());

        appController.attachPatchesInstaller(&patchesInstaller);

        SdlInputManager inputManager;
        appController.setSdlInputManager(&inputManager);

        ThemeManager themeManager;
        // Scan user themes directory under root
        themeManager.scanThemes(Paths::themesDir());
        // Also scan bundled themes relative to executable (works for both
        // bare exe and .app bundle layouts).
        QString bundledThemes = resolveResourceDir("themes");
        if (QDir(bundledThemes).exists() && bundledThemes != Paths::themesDir()) {
            themeManager.scanThemes(bundledThemes);
        }

        ThemeContext themeContext(&appController, &gameModel, &db);

        // Backfill serials for games imported before schema v6
        appController.backfillSerials();

        // Auto-scan ROM folders on startup so games appear immediately
        appController.scanRomFolders();

        SettingsTheme settingsTheme;

        QQmlApplicationEngine engine;
        engine.addImportPath("qrc:/");  // see SetupWizard engine above
        engine.rootContext()->setContextProperty("SettingsTheme", &settingsTheme);
        engine.rootContext()->setContextProperty("Theme", &theme);
        engine.rootContext()->setContextProperty("app", &appController);
        appController.setQmlEngine(&engine);
        engine.rootContext()->setContextProperty("gameModel", &gameModel);
        engine.rootContext()->setContextProperty("inputManager", &inputManager);
        engine.rootContext()->setContextProperty("themeManager", &themeManager);
        engine.rootContext()->setContextProperty("themeContext", &themeContext);
        engine.loadFromModule("AppUI", "AppWindow");

        if (engine.rootObjects().isEmpty()) {
            qCritical() << "Failed to load QML app";
            db.close();
            return 1;
        }

        // Pass the QML window to SdlInputManager for key event injection
        QWindow* window = qobject_cast<QWindow*>(engine.rootObjects().first());
        if (window)
            inputManager.setWindow(window);

        // Hide cursor globally — QML will restore it when settings overlay opens
        QGuiApplication::setOverrideCursor(Qt::BlankCursor);

        // Hide macOS menu bar and Dock so they never appear on mouse hover
        MacFullscreen::hideMenuBarAndDock();

        int ret = app.exec();

        // On quit, cleanly stop any still-running game BEFORE the QML engine
        // and static objects tear down. A core left running (e.g. Cmd+Q from
        // in-game) has joinable worker/emulation threads whose destructors
        // call std::terminate() during process static-destruction — Dolphin's
        // static s_emu_thread aborts this way (SIGABRT in __cxa_finalize) —
        // and the core's save-on-exit flush (memory cards / SRAM) is skipped.
        // stopGame() is async (CoreRuntime::finished is posted to the main
        // thread), so pump events until the session reports stopped. Bounded
        // so a misbehaving core can't hang exit. General to every libretro
        // core, not just Dolphin.
        if (GameSession* gs = appController.gameSession(); gs && gs->isRunning()) {
            qInfo() << "[main] stopping running game before exit";
            appController.stopGame();
            QDeadlineTimer deadline(5000);
            while (gs->isRunning() && !deadline.hasExpired())
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            if (gs->isRunning()) {
                // A wedged core still owns live threads inside its dylib.
                // Running static destructors under them segfaults (observed
                // 2026-07-03: ~GSTextureCache in __cxa_finalize during exit()
                // after a PCSX2 shutdown deadlock). Skip destructors entirely:
                // SQLite is crash-safe (journal), and the OS reclaims the rest.
                qWarning() << "[main] game did not stop within 5s; hard-exiting"
                              " (skipping static destructors — wedged core)";
                fflush(nullptr);
                ::_exit(ret);
            }
        }

        db.close();

        // A wedged core's dylib stays mapped (see CoreRuntime teardown), so
        // normal exit() would run ITS static destructors under live detached
        // threads — the exact ~GSTextureCache segv this replaces. Everything
        // of ours is already flushed (db closed above); skip the destructors.
        if (CoreRuntime::anyCoreWedged()) {
            qWarning() << "[main] a core wedged this session — hard exit"
                          " (skipping process static destructors)";
            fflush(nullptr);
            ::_exit(ret);
        }
        return ret;
    }
}

// ============================================================================
// CLI mode (unchanged)
// ============================================================================

static int runCli(QCoreApplication& /*app*/, QCommandLineParser& parser, ManifestLoader& loader) {
    qInfo() << "\n=== Emulator Status ===";
    for (const auto& emu : loader.allEmulators()) {
        auto* adapter = AdapterRegistry::instance().adapterFor(emu.id);
        if (!adapter) {
            qWarning() << emu.name << "- no adapter";
            continue;
        }

        bool installed = adapter->isInstalled(emu);
        QString installPath = Paths::emulatorsDir(emu.install_folder);
        QString execPath = adapter->resolveExecutable(emu, installPath);

        qInfo().noquote() << QString("  %1 [%2]: %3")
            .arg(emu.name, -15)
            .arg(emu.systems.join(", "))
            .arg(installed ? "INSTALLED  -> " + execPath : "NOT INSTALLED");
    }

    if (parser.isSet("install")) {
        const QString emuId = parser.value("install");
        const EmulatorManifest* manifest = loader.emulatorById(emuId);
        if (!manifest) { qCritical() << "Unknown emulator id:" << emuId; return 1; }

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) { qCritical() << "No adapter for:" << emuId; return 1; }

        if (adapter->isInstalled(*manifest)) {
            qInfo() << manifest->name << "is already installed.";
            return 0;
        }

        qInfo() << "\n=== Installing" << manifest->name << "===";

        EmulatorService emuService(&loader);
        auto result = emuService.installEmulatorSync(emuId);
        if (!result.success) {
            qCritical().noquote() << "Installation failed:" << result.message;
            return 1;
        }

        qInfo().noquote() << result.message;
        if (!result.version.isEmpty())
            qInfo().noquote() << "Version:" << result.version;

        const QString installPath = Paths::emulatorsDir(manifest->install_folder);
        const QString execPath = adapter->resolveExecutable(*manifest, installPath);
        qInfo() << "Executable:" << execPath << (QFileInfo::exists(execPath) ? "(exists)" : "(MISSING)");
    }

    if (parser.isSet("launch")) {
        const QString emuId = parser.value("launch");
        const QString romPath = parser.value("rom");

        if (romPath.isEmpty()) { qCritical() << "Usage: --launch <id> --rom <path>"; return 1; }

        const EmulatorManifest* manifest = loader.emulatorById(emuId);
        if (!manifest) { qCritical() << "Unknown emulator id:" << emuId; return 1; }

        auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
        if (!adapter) { qCritical() << "No adapter for:" << emuId; return 1; }

        if (!QFileInfo::exists(romPath)) { qCritical() << "ROM not found:" << romPath; return 1; }

        const QString installPath = Paths::emulatorsDir(manifest->install_folder);
        const QString execPath = QFileInfo(adapter->resolveExecutable(*manifest, installPath)).absoluteFilePath();
        if (!QFileInfo::exists(execPath)) {
            qCritical() << manifest->name << "not installed. Executable not found:" << execPath;
            return 1;
        }

        const QString systemId = Paths::systemIdFor(manifest->id, manifest->systems);
        const QString biosPath = QFileInfo(Paths::biosDir()).absoluteFilePath();
        const QString dataPath = QFileInfo(Paths::emulatorDataDir(emuId, systemId)).absoluteFilePath();
        QDir().mkpath(dataPath);
        if (!adapter->ensureConfig(*manifest, biosPath, dataPath))
            qWarning() << "[CLI] Config creation/patching failed for" << manifest->name;

        QStringList args = adapter->buildLaunchArgs(*manifest, romPath);

        QString cwd;
#if defined(Q_OS_MACOS)
        static const QRegularExpression appRe("^(.+\\.app)/");
        auto match = appRe.match(execPath);
        if (match.hasMatch())
            cwd = QFileInfo(match.captured(1)).absolutePath();
#endif
        if (cwd.isEmpty())
            cwd = QFileInfo(execPath).absolutePath();

        qInfo() << "\n=== Launching" << manifest->name << "===";
        qInfo().noquote() << execPath << args.join(" ");

        QProcess proc;
        proc.setWorkingDirectory(cwd);
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(execPath, args);

        if (!proc.waitForStarted(5000)) {
            qCritical() << "Failed to start process:" << proc.errorString();
            return 1;
        }
        qInfo() << "[CLI] PID:" << proc.processId();

        while (proc.state() != QProcess::NotRunning) {
            proc.waitForReadyRead(1000);
            QByteArray out = proc.readAll();
            if (!out.isEmpty()) {
                for (const auto& line : out.split('\n'))
                    if (!line.trimmed().isEmpty())
                        qInfo().noquote() << "  [" + emuId + "]" << line.trimmed();
            }
        }
        QByteArray remaining = proc.readAll();
        for (const auto& line : remaining.split('\n'))
            if (!line.trimmed().isEmpty())
                qInfo().noquote() << "  [" + emuId + "]" << line.trimmed();

        if (proc.exitStatus() == QProcess::CrashExit) {
            qCritical() << manifest->name << "crashed.";
            return 1;
        }
        return proc.exitCode();
    }

    return 0;
}
