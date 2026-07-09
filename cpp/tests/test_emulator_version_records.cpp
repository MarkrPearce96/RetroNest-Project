// cpp/tests/test_emulator_version_records.cpp
//
// Packet 6 regression guard: libretro version tracking is per-core.
// All libretro manifests share install_folder "libretro"; before packet 6
// EmulatorService kept ONE shared {root}/emulators/libretro/.version.json,
// so installing core A stamped core B's version and corrupted update
// detection suite-wide. Now each core owns a JSON sidecar
// cores/<core_dylib>.version, with a warned one-time fallback to the
// legacy shared file for pre-migration installs. Process-backend
// emulators keep the per-install-dir .version.json unchanged.

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "core/manifest_loader.h"
#include "core/paths.h"
#include "services/emulator_installer.h"
#include "services/emulator_service.h"

class TestEmulatorVersionRecords : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_root;       // fake {root}
    QTemporaryDir m_manifests;  // fake manifests dir
    ManifestLoader m_loader;

    static void writeFile(const QString& path, const QByteArray& bytes) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
        f.write(bytes);
    }

    QString coresDir() const { return m_root.path() + "/emulators/libretro/cores"; }
    QString legacyJsonPath() const { return m_root.path() + "/emulators/libretro/.version.json"; }

    static QJsonObject readJson(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return QJsonDocument::fromJson(f.readAll()).object();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_root.isValid());
        QVERIFY(m_manifests.isValid());
        QVERIFY(Paths::setRoot(m_root.path()));

        // Two libretro cores sharing install_folder "libretro".
        // (The process-backend fixture died with the process era — the
        // loader rejects non-libretro manifests now.)
        writeFile(m_manifests.path() + "/corea.json", R"({
            "id":"corea","name":"Core A","systems":["sa"],"github_repo":"o/a",
            "backend":"libretro","core_dylib":"corea_libretro.dylib",
            "executable":"corea_libretro.dylib","install_folder":"libretro",
            "rom_extensions":["bin"],"launch_args":[]
        })");
        writeFile(m_manifests.path() + "/coreb.json", R"({
            "id":"coreb","name":"Core B","systems":["sb"],"github_repo":"o/b",
            "backend":"libretro","core_dylib":"coreb_libretro.dylib",
            "executable":"coreb_libretro.dylib","install_folder":"libretro",
            "rom_extensions":["bin"],"launch_args":[]
        })");
        QVERIFY(m_loader.loadAll(m_manifests.path()));
    }

    void testLibretroSaveWritesPerDylibSidecar() {
        EmulatorService svc(&m_loader);
        svc.saveVersion("corea", "v1.0", "2026-01-01T00:00:00Z");

        const QString sidecar = coresDir() + "/corea_libretro.dylib.version";
        QVERIFY(QFile::exists(sidecar));
        const QJsonObject rec = readJson(sidecar);
        QCOMPARE(rec.value("version").toString(), QString("v1.0"));
        QCOMPARE(rec.value("published_at").toString(), QString("2026-01-01T00:00:00Z"));
        QVERIFY(!rec.value("installed_at").toString().isEmpty());

        // The legacy shared file must NOT be written for libretro cores.
        QVERIFY(!QFile::exists(legacyJsonPath()));

        QCOMPARE(svc.installedVersion("corea"), QString("v1.0"));
        QCOMPARE(svc.installedPublishedAt("corea"), QString("2026-01-01T00:00:00Z"));
        QVERIFY(!svc.installedAt("corea").isEmpty());
    }

    void testNoCrossCoreContamination() {
        EmulatorService svc(&m_loader);
        svc.saveVersion("corea", "v2.0", "2026-02-02T00:00:00Z");
        svc.saveVersion("coreb", "v9.9", "2026-03-03T00:00:00Z");

        // Installing B must not disturb A's record — the original bug.
        QCOMPARE(svc.installedVersion("corea"), QString("v2.0"));
        QCOMPARE(svc.installedVersion("coreb"), QString("v9.9"));
        QCOMPARE(svc.installedPublishedAt("corea"), QString("2026-02-02T00:00:00Z"));
        QCOMPARE(svc.installedPublishedAt("coreb"), QString("2026-03-03T00:00:00Z"));
    }

    void testLegacyFallbackWhenSidecarMissing() {
        // Fresh state: remove sidecars, plant only the legacy shared file.
        QFile::remove(coresDir() + "/corea_libretro.dylib.version");
        QFile::remove(coresDir() + "/coreb_libretro.dylib.version");
        writeFile(legacyJsonPath(),
                  R"({"version":"legacy-tag","published_at":"2025-12-12T00:00:00Z",)"
                  R"("installed_at":"2025-12-13T00:00:00Z"})");

        EmulatorService svc(&m_loader);
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("corea.*legacy shared \\.version\\.json")));
        QCOMPARE(svc.installedVersion("corea"), QString("legacy-tag"));
        // Warning fires once per core, not on every read.
        QCOMPARE(svc.installedPublishedAt("corea"), QString("2025-12-12T00:00:00Z"));
        QCOMPARE(svc.installedAt("corea"), QString("2025-12-13T00:00:00Z"));
    }

    void testOldRawTextSidecarFallsBackToLegacy() {
        // Pre-packet-6 installers wrote the sidecar as a bare published_at
        // string. Not a JSON object → treated like a missing sidecar.
        writeFile(coresDir() + "/corea_libretro.dylib.version",
                  "2026-05-27T10:18:33Z");
        writeFile(legacyJsonPath(), R"({"version":"shared-v3"})");

        EmulatorService svc(&m_loader);
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("corea.*legacy shared \\.version\\.json")));
        QCOMPARE(svc.installedVersion("corea"), QString("shared-v3"));
    }

    void testSidecarWinsOverLegacyOnceWritten() {
        writeFile(legacyJsonPath(), R"({"version":"stale-shared"})");
        EmulatorService svc(&m_loader);
        svc.saveVersion("corea", "v4.0", "2026-04-04T00:00:00Z");
        QCOMPARE(svc.installedVersion("corea"), QString("v4.0"));
    }

    void testNoRecordAnywhereReturnsEmpty() {
        QFile::remove(coresDir() + "/coreb_libretro.dylib.version");
        QFile::remove(legacyJsonPath());
        EmulatorService svc(&m_loader);
        QVERIFY(svc.installedVersion("coreb").isEmpty());
        QVERIFY(svc.installedPublishedAt("coreb").isEmpty());
        QVERIFY(svc.installedAt("coreb").isEmpty());
    }

    // Final-review finding: the private-manifest/empty-token guard in
    // EmulatorInstaller::fetchReleaseInfo must fail fast (no network I/O)
    // rather than let an unauthenticated request hit a private repo. This
    // exercises the guard through the public installSync entry point,
    // since fetchReleaseInfo itself is private. In this test binary
    // GitHubCredentials::token() always returns "" (no dev_credentials.cmake
    // define), so a private manifest unconditionally hits the guard.
    void testInstallSyncFailsFastForPrivateManifestWithNoToken() {
        EmulatorManifest manifest;
        manifest.id = "privatecore";
        manifest.name = "Private Core";
        manifest.github_repo = "acme/private-core";
        manifest.is_private = true;

        QTemporaryDir installDir;
        QVERIFY(installDir.isValid());

        EmulatorInstaller::InstallResult result =
            EmulatorInstaller::installSync(manifest, installDir.path());

        QVERIFY(!result.success);
        QVERIFY2(result.message.contains("requires a GitHub token"),
                  qPrintable(result.message));
    }

};

QTEST_MAIN(TestEmulatorVersionRecords)
#include "test_emulator_version_records.moc"
