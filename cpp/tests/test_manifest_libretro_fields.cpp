#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QRegularExpression>
#include "core/manifest_loader.h"

class TestManifestLibretroFields : public QObject {
    Q_OBJECT
private:
    QString writeManifest(const QString& dir, const QString& name, const QString& json) {
        const QString path = dir + "/" + name;
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(json.toUtf8());
        f.close();
        return path;
    }
private slots:
    void testBackendDefaultsToProcess() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "x.json", R"({
            "id":"x","name":"X","systems":["s"],"github_repo":"o/r",
            "executable":"X","install_folder":"x","rom_extensions":["bin"],"launch_args":["{rom_path}"]
        })");
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("x");
        QVERIFY(m != nullptr);
        QCOMPARE(m->backend, QString("process"));
        QVERIFY(m->core_dylib.isEmpty());
        QVERIFY(m->core_buildbot_path.isEmpty());
    }
    void testLibretroBackendFieldsParse() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "y.json", R"({
            "id":"y","name":"Y","systems":["s"],"github_repo":"o/r","backend":"libretro",
            "core_dylib":"y_libretro.dylib","core_buildbot_path":"y_libretro.dylib.zip",
            "executable":"y_libretro.dylib","install_folder":"libretro","rom_extensions":["bin"],"launch_args":[]
        })");
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("y");
        QVERIFY(m != nullptr);
        QCOMPARE(m->backend, QString("libretro"));
        QCOMPARE(m->core_dylib, QString("y_libretro.dylib"));
        QCOMPARE(m->core_buildbot_path, QString("y_libretro.dylib.zip"));
        // core_arch not declared → empty ("undeclared")
        QVERIFY(m->core_arch.isEmpty());
    }
    void testCoreArchParsesValidValues() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "u.json", R"({
            "id":"u","name":"U","systems":["s"],"github_repo":"o/r","backend":"libretro",
            "core_dylib":"u_libretro.dylib","core_arch":"universal",
            "executable":"u_libretro.dylib","install_folder":"libretro","rom_extensions":["bin"],"launch_args":[]
        })");
        writeManifest(dir.path(), "v.json", R"({
            "id":"v","name":"V","systems":["s"],"github_repo":"o/r","backend":"libretro",
            "core_dylib":"v_libretro.dylib","core_arch":"x86_64",
            "executable":"v_libretro.dylib","install_folder":"libretro","rom_extensions":["bin"],"launch_args":[]
        })");
        writeManifest(dir.path(), "w.json", R"({
            "id":"w","name":"W","systems":["s"],"github_repo":"o/r","backend":"libretro",
            "core_dylib":"w_libretro.dylib","core_arch":"arm64",
            "executable":"w_libretro.dylib","install_folder":"libretro","rom_extensions":["bin"],"launch_args":[]
        })");
        ManifestLoader loader;
        loader.loadAll(dir.path());
        QCOMPARE(loader.emulatorById("u")->core_arch, QString("universal"));
        QCOMPARE(loader.emulatorById("v")->core_arch, QString("x86_64"));
        QCOMPARE(loader.emulatorById("w")->core_arch, QString("arm64"));
    }
    void testCoreArchInvalidValueTreatedAsUndeclared() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "z.json", R"({
            "id":"z","name":"Z","systems":["s"],"github_repo":"o/r","backend":"libretro",
            "core_dylib":"z_libretro.dylib","core_arch":"armv7",
            "executable":"z_libretro.dylib","install_folder":"libretro","rom_extensions":["bin"],"launch_args":[]
        })");
        ManifestLoader loader;
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("invalid core_arch.*armv7")));
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("z");
        QVERIFY(m != nullptr);            // manifest still loads
        QVERIFY(m->core_arch.isEmpty());  // invalid value degrades to undeclared
    }
};
QTEST_MAIN(TestManifestLibretroFields)
#include "test_manifest_libretro_fields.moc"
