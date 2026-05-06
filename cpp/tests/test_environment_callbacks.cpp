#include <QtTest>
#include "core/libretro/environment_callbacks.h"
#include "core/libretro/options_store.h"

class TestEnvironmentCallbacks : public QObject {
    Q_OBJECT
private slots:
    void testSetPixelFormatRgb565() {
        EnvironmentContext ctx;
        ctx.systemDirectory = "/bios";
        ctx.saveDirectory = "/save";
        retro_pixel_format pf = RETRO_PIXEL_FORMAT_RGB565;
        QVERIFY(environmentDispatch(&ctx, RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pf));
        QCOMPARE(static_cast<int>(ctx.pixelFormat), static_cast<int>(RETRO_PIXEL_FORMAT_RGB565));
    }
    void testGetSystemDirectory() {
        EnvironmentContext ctx; ctx.systemDirectory = "/bios";
        const char* outPath = nullptr;
        QVERIFY(environmentDispatch(&ctx, RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &outPath));
        QCOMPARE(QString(outPath), QString("/bios"));
    }
    void testGetSaveDirectory() {
        EnvironmentContext ctx; ctx.saveDirectory = "/save";
        const char* outPath = nullptr;
        QVERIFY(environmentDispatch(&ctx, RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &outPath));
        QCOMPARE(QString(outPath), QString("/save"));
    }
    void testGetVariableReadsFromOptionsStore() {
        EnvironmentContext ctx;
        OptionsStore s;
        s.load(":memory:", {{"k","K","v0",{"v0","v1"}}});
        s.set("k","v1");
        ctx.options = &s;
        retro_variable v{ "k", nullptr };
        QVERIFY(environmentDispatch(&ctx, RETRO_ENVIRONMENT_GET_VARIABLE, &v));
        QCOMPARE(QString(v.value), QString("v1"));
    }
    void testVariableUpdateClearsAfterRead() {
        EnvironmentContext ctx;
        OptionsStore s; s.load(":memory:", {{"k","K","v0",{"v0","v1"}}});
        s.set("k","v1");
        ctx.options = &s;
        bool updated = false;
        QVERIFY(environmentDispatch(&ctx, RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated));
        QCOMPARE(updated, true);
        QVERIFY(environmentDispatch(&ctx, RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated));
        QCOMPARE(updated, false);
    }
    void testUnknownEnumReturnsFalse() {
        EnvironmentContext ctx;
        QVERIFY(!environmentDispatch(&ctx, 99999u, nullptr));
    }
};
QTEST_MAIN(TestEnvironmentCallbacks)
#include "test_environment_callbacks.moc"
