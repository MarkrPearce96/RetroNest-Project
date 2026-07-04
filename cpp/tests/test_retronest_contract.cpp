// ABI pin for the retronest-libretro contract package. These numeric values
// and layouts are burned into shipped core dylibs — if this test fails, you
// renumbered the contract (a cross-repo breaking change), not the test.
#include <QtTest>
#include <cstddef>
#include "retronest_libretro.h"

class TestRetronestContract : public QObject {
    Q_OBJECT
private slots:
    void commandValues() {
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW),   0x20001u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH), 0x20002u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR),   0x20003u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR),   0x20004u);
        QCOMPARE(unsigned(RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY),  0x20005u);
    }
    void identityStructLayout() {
        QCOMPARE(sizeof(retronest_game_identity), 2 * sizeof(const char*));
        QCOMPARE(offsetof(retronest_game_identity, ra_hash), size_t(0));
        QCOMPARE(offsetof(retronest_game_identity, serial), sizeof(const char*));
    }
    void exportTypedefsCompile() {
        retronest_set_paused_t p = nullptr;
        retronest_set_fast_forward_t f = nullptr;
        retronest_shutdown_wedged_t w = nullptr;
        QVERIFY(p == nullptr && f == nullptr && w == nullptr);
    }
};
QTEST_APPLESS_MAIN(TestRetronestContract)
#include "test_retronest_contract.moc"
