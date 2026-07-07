// cpp/tests/test_theme_manager.cpp
//
// ThemeManager scan/selection contract:
//  - theme choice persists across launches via config.json (app-shell
//    review D1: ThemesPage wrote currentThemeId, nothing saved it, next
//    launch silently reverted to the first scanned theme);
//  - the persisted choice wins over first-theme fallback and is applied
//    even when its theme only appears on the second scan pass (user
//    themes dir is scanned before the bundled dir);
//  - config.json writers read-modify-write, so saveRoot can't clobber
//    the saved theme or vice versa.

#include <QtTest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include "ui/theme_manager.h"
#include "core/paths.h"

class TestThemeManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        // minAppVersion checks compare against this (main.cpp sets 0.1.0).
        QCoreApplication::setApplicationVersion("0.1.0");
    }

    void init() {
        QFile::remove(Paths::appConfigPath());
    }

    void firstThemeFallbackWhenNothingSaved() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha");
        makeTheme(dir.path(), "beta");

        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        QCOMPARE(mgr.currentThemeId(), QString("alpha"));
    }

    void savedThemeWinsOverFallback() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha");
        makeTheme(dir.path(), "beta");
        Paths::saveTheme("beta");

        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        QCOMPARE(mgr.currentThemeId(), QString("beta"));
    }

    void savedThemeAppliedOnSecondScanPass() {
        QTemporaryDir userDir;
        QTemporaryDir bundledDir;
        makeTheme(userDir.path(), "alpha");
        makeTheme(bundledDir.path(), "modern");
        Paths::saveTheme("modern");

        ThemeManager mgr;
        mgr.scanThemes(userDir.path());
        QCOMPARE(mgr.currentThemeId(), QString("alpha"));  // saved not seen yet

        QSignalSpy spy(&mgr, &ThemeManager::currentThemeChanged);
        mgr.scanThemes(bundledDir.path());
        QCOMPARE(mgr.currentThemeId(), QString("modern"));
        QCOMPARE(spy.count(), 1);
    }

    void missingSavedThemeFallsBack() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha");
        Paths::saveTheme("deleted-theme");

        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        QCOMPARE(mgr.currentThemeId(), QString("alpha"));
    }

    void setCurrentThemeIdPersists() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha");
        makeTheme(dir.path(), "beta");

        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        mgr.setCurrentThemeId("beta");
        QCOMPARE(Paths::loadSavedTheme(), QString("beta"));

        // Rejected ids (unknown theme) must not overwrite the saved choice.
        mgr.setCurrentThemeId("nonexistent");
        QCOMPARE(Paths::loadSavedTheme(), QString("beta"));
    }

    // ── theme.json scan-time validation (review R9) ──

    void minAppVersionTooHighRejected() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha");
        makeTheme(dir.path(), "future", "\"minAppVersion\": \"99.0\",\n");

        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        QCOMPARE(mgr.availableThemes().size(), 1);
        QCOMPARE(mgr.currentThemeId(), QString("alpha"));
    }

    void minAppVersionSatisfiedAccepted() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha", "\"minAppVersion\": \"0.1.0\",\n");

        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        QCOMPARE(mgr.availableThemes().size(), 1);
    }

    void missingPageFileRejectedAtScan() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha");
        makeTheme(dir.path(), "broken");
        QVERIFY(QFile::remove(dir.path() + "/broken/GameList.qml"));

        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        QCOMPARE(mgr.availableThemes().size(), 1);
        QCOMPARE(mgr.currentThemeId(), QString("alpha"));
    }

    void unknownKeysWarnButAccept() {
        QTemporaryDir dir;
        makeTheme(dir.path(), "alpha", "\"typoKey\": true,\n");

        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("unknown key.*typoKey"));
        ThemeManager mgr;
        mgr.scanThemes(dir.path());
        QCOMPARE(mgr.availableThemes().size(), 1);
    }

    void saveRootPreservesSavedTheme() {
        Paths::saveTheme("beta");
        Paths::saveRoot("/tmp/retronest-test-root");
        QCOMPARE(Paths::loadSavedTheme(), QString("beta"));
        QCOMPARE(Paths::loadSavedRoot(), QString("/tmp/retronest-test-root"));

        Paths::saveTheme("alpha");
        QCOMPARE(Paths::loadSavedRoot(), QString("/tmp/retronest-test-root"));
    }

private:
    // Creates <base>/<id>/theme.json with the required pages, and the page
    // files themselves so scan-time validation passes. `extraJson` is
    // spliced in as additional top-level "key": value, lines.
    void makeTheme(const QString& base, const QString& id,
                   const QByteArray& extraJson = {}) {
        const QString themeDir = base + "/" + id;
        QVERIFY(QDir().mkpath(themeDir));

        QFile sys(themeDir + "/SystemBrowser.qml");
        QVERIFY(sys.open(QIODevice::WriteOnly));
        sys.close();
        QFile games(themeDir + "/GameList.qml");
        QVERIFY(games.open(QIODevice::WriteOnly));
        games.close();

        QFile f(themeDir + "/theme.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(
            "{\n"
            "  \"name\": \"") + id.toUtf8() + QByteArray("\",\n"
            "  \"version\": \"1.0\",\n"
            "  \"author\": \"Test\",\n") + extraJson + QByteArray(
            "  \"pages\": {\n"
            "    \"systemBrowser\": \"SystemBrowser.qml\",\n"
            "    \"gameList\": \"GameList.qml\"\n"
            "  }\n"
            "}\n"));
        f.close();
    }
};

QTEST_GUILESS_MAIN(TestThemeManager)
#include "test_theme_manager.moc"
