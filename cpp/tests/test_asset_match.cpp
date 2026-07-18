#include <QtTest>
#include "adapters/emulator_adapter.h"

// Exercises EmulatorAdapter::matchAsset — the release-asset selector that
// gates every core download. The load-bearing property: rule order in
// assetMatchRules() is PREFERENCE order, regardless of how GitHub happens
// to order the release's assets (per-arch zips coexist with the historical
// fallback since the pcsx2 native-arm transition).

namespace {

class StubAdapter : public EmulatorAdapter {
public:
    QVector<AssetMatchRule> rules;

    QVector<AssetMatchRule> assetMatchRules() const override { return rules; }

    bool ensureConfig(const EmulatorManifest&, const QString&,
                      const QString&) override { return true; }
    QString resolveExecutable(const EmulatorManifest&,
                              const QString& installPath) override {
        return installPath;
    }
};

// Mirrors LibretroAdapter's macOS-arm64 rule set.
QVector<EmulatorAdapter::AssetMatchRule> arm64PreferredRules()
{
    return {
        EmulatorAdapter::AssetMatchRule{ {QStringLiteral("-macos-arm64")}, ".zip" },
        EmulatorAdapter::AssetMatchRule{ {}, ".dylib.zip" },
    };
}

} // namespace

class TestAssetMatch : public QObject {
    Q_OBJECT
private slots:
    void preferredRuleBeatsAssetListingOrder() {
        StubAdapter a;
        a.rules = arm64PreferredRules();
        // Fallback listed FIRST — the preferred rule must still win.
        const QStringList assets{ "pcsx2_libretro.dylib.zip",
                                  "pcsx2_libretro-macos-arm64.zip" };
        QCOMPARE(a.matchAsset(assets), QString("pcsx2_libretro-macos-arm64.zip"));
    }

    void fallbackWhenNoPerArchAsset() {
        StubAdapter a;
        a.rules = arm64PreferredRules();
        const QStringList assets{ "pcsx2_libretro.dylib.zip" };
        QCOMPARE(a.matchAsset(assets), QString("pcsx2_libretro.dylib.zip"));
    }

    void perArchZipNeverMatchesFallbackRule() {
        StubAdapter a;
        // x86 host shape: fallback rule only. The arm64 zip must NOT match
        // (its name deliberately doesn't end in ".dylib.zip").
        a.rules = { EmulatorAdapter::AssetMatchRule{ {}, ".dylib.zip" } };
        const QStringList assets{ "pcsx2_libretro-macos-arm64.zip",
                                  "pcsx2_libretro.dylib.zip" };
        QCOMPARE(a.matchAsset(assets), QString("pcsx2_libretro.dylib.zip"));
    }

    void allSubstringsRequired() {
        StubAdapter a;
        a.rules = { EmulatorAdapter::AssetMatchRule{
            {QStringLiteral("nightly"), QStringLiteral("arm64")}, ".zip" } };
        // Names avoid the "mac" keyword so matchAsset's generic
        // platform-keyword fallback (which runs after the rules) stays out
        // of the picture — this test targets rule-substring semantics only.
        QCOMPARE(a.matchAsset({ "core-nightly-x86_64.zip" }), QString());
        QCOMPARE(a.matchAsset({ "core-nightly-arm64.zip" }),
                 QString("core-nightly-arm64.zip"));
    }
};

QTEST_MAIN(TestAssetMatch)
#include "test_asset_match.moc"
