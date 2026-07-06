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

    // ── Process-era retirement: only libretro-backend manifests load ──

    void testProcessBackendRejected() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "p.json", R"({
            "manifest_version":1,"id":"p","name":"P","systems":["s"],"github_repo":"o/r",
            "executable":"P","rom_extensions":["bin"],"launch_args":[],
            "backend":"process"
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("unsupported backend.*process")));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        QVERIFY(loader.emulatorById("p") == nullptr);   // rejected, not defaulted
    }
    void testMissingBackendRejected() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "m.json", R"({
            "manifest_version":1,"id":"m","name":"M","systems":["s"],"github_repo":"o/r",
            "executable":"M","rom_extensions":["bin"],"launch_args":[]
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("unsupported backend")));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        QVERIFY(loader.emulatorById("m") == nullptr);
    }

    // ── Packet 7 Stage 3: version stamp, logo, detail_page, typo net ──

    void testDetailPageFieldsParse() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "d.json", R"({
            "manifest_version":1,"id":"d","name":"D","systems":["s"],"github_repo":"o/r",
            "executable":"D","rom_extensions":["bin"],"launch_args":[],
            "backend":"libretro","core_dylib":"d_libretro.dylib",
            "logo":"qrc:/AppUI/qml/AppUI/images/dolphin_logo.png",
            "detail_page":{
                "controller_pages":[{"label":"GameCube Controller","type":"GCPad1"},
                                     {"label":"Wii Classic Controller","type":"Wiimote1"}],
                "has_patches":true}
        })");
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("d");
        QVERIFY(m != nullptr);
        QCOMPARE(m->manifest_version, 1);
        QCOMPARE(m->logo, QString("qrc:/AppUI/qml/AppUI/images/dolphin_logo.png"));
        QCOMPARE(m->controller_pages.size(), 2);
        QCOMPARE(m->controller_pages[0].label, QString("GameCube Controller"));
        QCOMPARE(m->controller_pages[0].type,  QString("GCPad1"));
        QCOMPARE(m->controller_pages[1].label, QString("Wii Classic Controller"));
        QCOMPARE(m->controller_pages[1].type,  QString("Wiimote1"));
        QVERIFY(m->has_patches);
    }
    void testDetailPageFieldsDefault() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "x.json", R"({
            "manifest_version":1,"id":"x","name":"X","systems":["s"],"github_repo":"o/r",
            "executable":"X","rom_extensions":["bin"],"launch_args":[],
            "backend":"libretro","core_dylib":"x_libretro.dylib"
        })");
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("x");
        QVERIFY(m != nullptr);
        QVERIFY(m->logo.isEmpty());
        QVERIFY(m->controller_pages.isEmpty());
        QVERIFY(!m->has_patches);
    }
    void testUnknownKeyWarns() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "u.json", R"({
            "manifest_version":1,"id":"u","name":"U","systems":["s"],"github_repo":"o/r",
            "executable":"U","rom_extensions":["bin"],"launch_args":[],
            "backend":"libretro","core_dylib":"u_libretro.dylib",
            "tpyo_field":true
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("unknown key.*tpyo_field")));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        QVERIFY(loader.emulatorById("u") != nullptr);   // warn, don't reject
    }
    void testMissingVersionWarns() {
        QTemporaryDir dir;
        writeManifest(dir.path(), "v.json", R"({
            "id":"v","name":"V","systems":["s"],"github_repo":"o/r",
            "executable":"V","rom_extensions":["bin"],"launch_args":[],
            "backend":"libretro","core_dylib":"v_libretro.dylib"
        })");
        QTest::ignoreMessage(QtWarningMsg,
            QRegularExpression(QStringLiteral("missing manifest_version")));
        ManifestLoader loader;
        loader.loadAll(dir.path());
        const auto* m = loader.emulatorById("v");
        QVERIFY(m != nullptr);                          // grandfathered, not rejected
        QCOMPARE(m->manifest_version, 0);
    }
};
QTEST_MAIN(TestManifestLibretroFields)
#include "test_manifest_libretro_fields.moc"
