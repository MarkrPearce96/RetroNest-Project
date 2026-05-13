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

    // SP6.5 Task 4.5: regression guard for the consumed-flag protocol.
    //
    // The original implementation called bootStatePath.clear() inside the
    // handler. That would have dangled *out for the caller because
    // toUtf8() returns a refcount=1 QByteArray; clear() frees its buffer
    // immediately. The fix introduced bootStatePathConsumed=true as the
    // consumed marker, leaving the QByteArray intact for the caller's
    // synchronous use. This test pins both invariants:
    //   1. The returned pointer remains usable after the call returns.
    //   2. A second call returns false (one-shot semantics).
    void testGetBootStatePathOneShot() {
        EnvironmentContext ctx;
        ctx.bootStatePath = QByteArray("/tmp/test.resume");
        ctx.bootStatePathConsumed = false;

        // First call hands off the path and marks consumed.
        const char* out1 = nullptr;
        QVERIFY(environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH, &out1));
        QVERIFY(out1 != nullptr);
        QCOMPARE(QByteArray(out1), QByteArray("/tmp/test.resume"));
        QCOMPARE(ctx.bootStatePathConsumed, true);

        // Pointer must STILL be valid (clear() was not called).
        QCOMPARE(ctx.bootStatePath, QByteArray("/tmp/test.resume"));
        QCOMPARE(QByteArray(out1), QByteArray("/tmp/test.resume"));

        // Second call: one-shot, returns false.
        const char* out2 = nullptr;
        QVERIFY(!environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH, &out2));
        QCOMPARE(out2, static_cast<const char*>(nullptr));
    }

    void testGetBootStatePathEmptyReturnsFalse() {
        EnvironmentContext ctx;
        // No path set; bootStatePathConsumed default-false.
        const char* out = nullptr;
        QVERIFY(!environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH, &out));
        QCOMPARE(out, static_cast<const char*>(nullptr));
        QCOMPARE(ctx.bootStatePathConsumed, false);
    }

    void testGetBootStatePathNullDataReturnsFalse() {
        EnvironmentContext ctx;
        ctx.bootStatePath = QByteArray("/tmp/test.resume");
        // Buggy core passes null data — handler must not crash, must return false.
        QVERIFY(!environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH, nullptr));
        // Path is unchanged; consumed flag NOT set.
        QCOMPARE(ctx.bootStatePath, QByteArray("/tmp/test.resume"));
        QCOMPARE(ctx.bootStatePathConsumed, false);
    }
};
QTEST_MAIN(TestEnvironmentCallbacks)
#include "test_environment_callbacks.moc"
