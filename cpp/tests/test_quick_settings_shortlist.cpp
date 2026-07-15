#include <QtTest>
#include <QFileInfo>
#include <QSet>
#include <optional>

#include "adapters/emulator_adapter.h"
#include "adapters/libretro/duckstation_libretro_adapter.h"
#include "adapters/libretro/pcsx2_libretro_adapter.h"
#include "adapters/libretro/dolphin_libretro_adapter.h"
#include "adapters/libretro/ppsspp_libretro_adapter.h"
#include "adapters/libretro/mgba_libretro_adapter.h"
#include "core/libretro/declared_options.h"
#include "core/setting_def.h"

// Guard: the quick Resolution / Aspect Ratio tabs curate a shortlist of
// core-option VALUES per adapter (resolutionOptionShortlist / aspectRatio…),
// plus widescreen on/off/trigger values. config_service silently skips any
// shortlist value the core doesn't declare — so a fork-side value rename would
// make pills vanish with no other failure. This pins every hand-written value
// against the committed declared fixture (the same convention the schema tests
// use), so a drift trips loudly here.
namespace {

QString fixtureDir() {
    return QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
           + "/fixtures/declared/";
}

std::optional<DeclaredOptionsDoc> loadDoc(const QString& core) {
    return DeclaredOptionsDoc::load(fixtureDir() + core + "_declared_options.json");
}

// The set of values the core declares for a given option key.
QSet<QString> declaredValues(const DeclaredOptionsDoc& doc, const QString& key) {
    QSet<QString> s;
    for (const auto& o : doc.options)
        if (o.key == key)
            for (const auto& v : o.values)
                s.insert(v.value);
    return s;
}

// Assert every value in a core-option shortlist is one the core declares.
void checkCoreShortlist(const QString& core, const QString& key,
                        const QVector<QPair<QString, QString>>& shortlist,
                        const char* what) {
    QVERIFY2(!key.isEmpty(),
             qPrintable(QString("%1: %2 option key is empty").arg(core, what)));
    const auto doc = loadDoc(core);
    QVERIFY2(doc.has_value(),
             qPrintable(QString("%1: declared fixture missing").arg(core)));
    const auto declared = declaredValues(*doc, key);
    QVERIFY2(!declared.isEmpty(),
             qPrintable(QString("%1: key '%2' not declared by the core").arg(core, key)));
    QVERIFY2(!shortlist.isEmpty(),
             qPrintable(QString("%1: %2 shortlist is empty").arg(core, what)));
    for (const auto& e : shortlist)
        QVERIFY2(declared.contains(e.first),
                 qPrintable(QString("%1 %2 shortlist value '%3' (key %4) is not "
                                    "declared by the core — stale/renamed?")
                                .arg(core, what, e.first, key)));
}

// Assert the widescreen on/off values are declared and the trigger value is a
// real shortlisted aspect choice.
void checkWidescreen(const QString& core, const EmulatorAdapter& a) {
    const QString wsKey = a.widescreenOptionKey();
    if (wsKey.isEmpty()) return;   // no coupling for this core
    const auto doc = loadDoc(core);
    QVERIFY2(doc.has_value(), qPrintable(QString("%1: fixture missing").arg(core)));
    const auto declared = declaredValues(*doc, wsKey);
    QVERIFY2(declared.contains(a.widescreenEnabledValue()),
             qPrintable(QString("%1: widescreen enabled value '%2' (key %3) not declared")
                            .arg(core, a.widescreenEnabledValue(), wsKey)));
    QVERIFY2(declared.contains(a.widescreenDisabledValue()),
             qPrintable(QString("%1: widescreen disabled value '%2' (key %3) not declared")
                            .arg(core, a.widescreenDisabledValue(), wsKey)));
    bool triggerShortlisted = false;
    for (const auto& e : a.aspectRatioOptionShortlist())
        if (e.first == a.widescreenTriggerValue()) { triggerShortlisted = true; break; }
    QVERIFY2(triggerShortlisted,
             qPrintable(QString("%1: widescreenTriggerValue '%2' is not one of the "
                                "aspect shortlist values").arg(core, a.widescreenTriggerValue())));
}

} // namespace

class TestQuickSettingsShortlist : public QObject {
    Q_OBJECT
private slots:
    void duckstation() {
        DuckStationLibretroAdapter a;
        checkCoreShortlist("duckstation", a.resolutionOptionKey(),
                           a.resolutionOptionShortlist(), "resolution");
        checkCoreShortlist("duckstation", a.aspectRatioOptionKey(),
                           a.aspectRatioOptionShortlist(), "aspect");
        checkWidescreen("duckstation", a);
    }

    void pcsx2() {
        Pcsx2LibretroAdapter a;
        checkCoreShortlist("pcsx2", a.resolutionOptionKey(),
                           a.resolutionOptionShortlist(), "resolution");
        checkCoreShortlist("pcsx2", a.aspectRatioOptionKey(),
                           a.aspectRatioOptionShortlist(), "aspect");
        checkWidescreen("pcsx2", a);
    }

    void dolphin() {
        DolphinLibretroAdapter a;
        checkCoreShortlist("dolphin", a.resolutionOptionKey(),
                           a.resolutionOptionShortlist(), "resolution");
        checkCoreShortlist("dolphin", a.aspectRatioOptionKey(),
                           a.aspectRatioOptionShortlist(), "aspect");
        checkWidescreen("dolphin", a);
    }

    void ppsspp_resolutionOnly() {
        PpssppLibretroAdapter a;
        checkCoreShortlist("ppsspp", a.resolutionOptionKey(),
                           a.resolutionOptionShortlist(), "resolution");
        // PPSSPP has no aspect ratio (core or frontend) — verify it stays off.
        QVERIFY2(a.aspectRatioOptionKey().isEmpty(), "ppsspp should have no aspect core option");
        QVERIFY2(a.aspectRatioFrontendKey().isEmpty(), "ppsspp should have no frontend aspect");
    }

    void mgba_frontendAspect() {
        MgbaLibretroAdapter a;
        // Aspect is a FRONTEND setting (no core option); validate its shortlist
        // against the frontend aspect_mode row the adapter declares.
        QVERIFY2(a.aspectRatioOptionKey().isEmpty(), "mgba aspect should not be a core option");
        QCOMPARE(a.aspectRatioFrontendKey(), QStringLiteral("aspect_mode"));

        QSet<QString> feVals;
        for (const auto& d : a.extraSettings())
            if (d.storage == SettingDef::Storage::FrontendSetting
                && d.key == a.aspectRatioFrontendKey())
                for (const auto& opt : d.options)
                    feVals.insert(opt.second);
        QVERIFY2(!feVals.isEmpty(), "mgba declares no aspect_mode frontend values");

        const auto shortlist = a.aspectRatioOptionShortlist();
        QVERIFY2(!shortlist.isEmpty(), "mgba aspect shortlist is empty");
        for (const auto& e : shortlist)
            QVERIFY2(feVals.contains(e.first),
                     qPrintable(QString("mgba aspect shortlist value '%1' is not a declared "
                                        "aspect_mode value").arg(e.first)));

        // No internal-resolution scaling on GBA — stay off the Resolution tab.
        QVERIFY2(a.resolutionOptionKey().isEmpty(), "mgba should have no resolution option");
    }
};

QTEST_MAIN(TestQuickSettingsShortlist)
#include "test_quick_settings_shortlist.moc"
