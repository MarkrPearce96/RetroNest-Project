#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
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
#include "core/system_registry.h"
#include "services/emulator_service.h"
#include "services/patches_installer.h"
#include "services/ra_service.h"
#include "services/scraper_service.h"
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

// Hard exit that SKIPS process static destructors. A wedged core's dylib
// stays mapped with live detached threads; running its static destructors
// under them segfaults (observed: ~GSTextureCache in __cxa_finalize after a
// PCSX2 shutdown deadlock). Everything of ours is flushed by the caller
// (SQLite is crash-safe anyway); the OS reclaims the rest.
[[noreturn]] static void guardedHardExit(int ret, const char* reason) {
    qWarning().noquote() << "[main]" << reason
                         << "— hard exit (skipping static destructors)";
    fflush(nullptr);
    ::_exit(ret);
}

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
    parser.addOption({"launch", "(retired) emulators launch from the GUI app", "id"});
    parser.addOption({"rom", "(retired) was the ROM path for --launch", "path"});
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
    // System-facts registry (packet 7 stage 3): display names, ScreenScraper
    // IDs, RA console IDs. Same directory, own file — loadAll skips it.
    if (!SystemRegistry::load(manifestsDir + "/systems.json")) {
        qCritical() << "Failed to load system registry from" << manifestsDir;
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
        // Null DB is safe here: loginWithPassword() (the only method the
        // wizard's RetroAchievements step calls) never touches m_db — it
        // only touches m_creds + network. Verified by reading ra_service.cpp.
        RAService raService(nullptr);
        // Null DB is safe here too: validateAndSaveCredentials() (the only
        // method the wizard's ScreenScraper step calls) never touches m_db —
        // it only touches m_scraper/m_creds + network. Verified by reading
        // scraper_service.cpp.
        ScraperService scraperService(nullptr);
        // Load credentials (mirrors AppController): seeds the compile-time
        // ScreenScraper DEV credentials (devid/devpassword/softname) that every
        // ssuserInfos API call requires — without this the wizard's scraper
        // login sends empty dev creds and ScreenScraper replies 403. Also loads
        // any saved RA token. m_creds.load() sets the compiled dev defaults even
        // when no config file exists yet (first run), so this is safe here.
        raService.loadCredentials();
        scraperService.loadCredentials();

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
        engine.rootContext()->setContextProperty("raService", &raService);
        engine.rootContext()->setContextProperty("scraperService", &scraperService);
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
    // ROM/BIOS roots may live outside the data root (e.g. USB). Empty ⇒ default.
    Paths::setRomsRoot(Paths::loadSavedRomsRoot());
    Paths::setBiosRoot(Paths::loadSavedBiosRoot());
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
        // Warn only after every pass — an empty user themes dir must not
        // false-alarm before the bundled pass loads Modern.
        themeManager.warnIfEmpty();

        ThemeContext themeContext(&appController, &gameModel, &db);

        // Backfill serials for games imported before schema v6
        appController.backfillSerials();

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
            // Under Launch Services there's no terminal — without this the
            // app just vanishes (or worse, leaves a black frameless window
            // if the failure happens later). Say something first.
            QMessageBox::critical(nullptr, QStringLiteral("RetroNest"),
                QStringLiteral("RetroNest failed to load its interface.\n\n"
                               "Run it from a terminal to see the QML error output."));
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

        // Auto-scan ROM folders shortly after the window is up. The game
        // list loads from the DB (populated by prior scans), so the UI
        // paints immediately; this filesystem re-scan is a background
        // refresh that picks up added/removed ROMs. Deferring it off the
        // pre-first-paint path keeps startup snappy (review P8).
        QTimer::singleShot(0, &appController, [&appController]() {
            appController.scanRomFolders();
        });

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
                db.close();  // flush ours before skipping destructors
                guardedHardExit(ret, "game did not stop within 5s (wedged core)");
            }
        }

        db.close();

        // A wedged core's dylib stays mapped (see CoreRuntime teardown), so
        // a normal return would run ITS static destructors under live
        // detached threads. Everything of ours is flushed (db closed above).
        if (CoreRuntime::anyCoreWedged())
            guardedHardExit(ret, "a core wedged this session");
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
        // Process-era retirement (2026-07): direct-launching an emulator
        // binary from the CLI is gone — every emulator is an in-process
        // libretro core that needs the GUI app's render/input/session
        // plumbing. Launch games from the app.
        qCritical() << "--launch was retired: all emulators are in-process libretro"
                       " cores; launch games from the RetroNest app.";
        return 1;
    }

    return 0;
}
