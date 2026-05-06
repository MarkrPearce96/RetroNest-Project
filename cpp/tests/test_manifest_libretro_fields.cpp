#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
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
    }
};
QTEST_MAIN(TestManifestLibretroFields)
#include "test_manifest_libretro_fields.moc"
