// cpp/tests/test_dolphin_libretro_schema.cpp
//
// Shape guard for DolphinLibretroAdapter's settings schema +
// settingsHubCards() + previewSpec(). Since Packet 7 Stage 2 the schema
// renders from the core's declared option table (committed fixture) × the
// adapter's curation overlay — this guard locks the merged shape: routing,
// sub-tabs, Recommended cross-listings, and structural invariants.

#include <QtTest>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include "adapters/libretro/dolphin_libretro_adapter.h"
#include "core/setting_def.h"

class TestDolphinLibretroSchema : public QObject {
    Q_OBJECT
    QVector<SettingDef> schema_;
private slots:
    void initTestCase() {
        DolphinLibretroAdapter adapter;
        // Packet 7 Stage 2: the schema renders from the core's declared
        // option table — hermetic tests inject the committed fixture
        // (recorded at conversion time by the retired test_schema_parity tool) instead of
        // touching the live sidecar / prober.
        const QString fixture = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/fixtures/declared/dolphin_declared_options.json";
        const auto doc = DeclaredOptionsDoc::load(fixture);
        QVERIFY2(doc.has_value(), "declared fixture missing");
        adapter.setDeclaredDocForTest(*doc);
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void totalCount_matchesOverlay() {
        // 90 curated core options (full coverage of the declared table) +
        // 16 Recommended cross-listings = 106 rows. A key the core stops
        // declaring is skipped by the merge with only a qWarning — this
        // count trips loud instead. New upstream options arrive hidden
        // (uncurated) and don't move this number until curated.
        QCOMPARE(schema_.size(), 106);
    }

    void everyLibretroKey_hasDolphinPrefix() {
        for (const auto& d : schema_)
            if (d.storage == SettingDef::Storage::LibretroOption)
                QVERIFY2(d.key.startsWith("dolphin_"),
                    qPrintable(QString("LibretroOption key '%1' missing dolphin_ prefix").arg(d.key)));
    }

    void everyRow_isLibretroCombo() {
        for (const auto& d : schema_) {
            QVERIFY2(d.storage == SettingDef::Storage::LibretroOption,
                qPrintable(QString("row '%1' is not LibretroOption storage").arg(d.key)));
            QVERIFY2(d.type == SettingDef::Combo,
                qPrintable(QString("row '%1' is not Combo type").arg(d.key)));
        }
    }

    void everyDefault_isInOptions() {
        for (const auto& d : schema_) {
            bool found = false;
            for (const auto& opt : d.options)
                if (opt.second == d.defaultValue) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("row '%1' default '%2' not in its options")
                                           .arg(d.key).arg(d.defaultValue)));
        }
    }

    void allFiveSubTabs_present() {
        QSet<QString> subs;
        for (const auto& d : schema_)
            if (d.category == "Graphics") subs.insert(d.subcategory);
        const QSet<QString> expect{"General","Enhancements","Hacks","Advanced","On-Screen Display"};
        QCOMPARE(subs, expect);
    }

    void noDuplicateKeysPerCategory() {
        // Recommended deliberately re-references keys that live in other
        // categories, so global uniqueness no longer holds — enforce
        // uniqueness within each category instead.
        QSet<QString> seen;  // "category/key"
        for (const auto& d : schema_) {
            const QString id = d.category + "/" + d.key;
            QVERIFY2(!seen.contains(id),
                qPrintable(QString("duplicate key '%1' in category '%2'").arg(d.key).arg(d.category)));
            seen.insert(id);
        }
    }

    void recommendedRows_haveAHomeElsewhere() {
        QSet<QString> nonRecKeys;
        for (const auto& d : schema_)
            if (d.category != "Recommended") nonRecKeys.insert(d.key);
        for (const auto& d : schema_)
            if (d.category == "Recommended")
                QVERIFY2(nonRecKeys.contains(d.key),
                    qPrintable(QString("Recommended row '%1' has no home row in another category").arg(d.key)));
    }

    void overclockGates_carryDependsOn() {
        // The two dependsOn gates the overlay must preserve: the multiplier
        // combos are greyed until their _enable toggles are on.
        QHash<QString, QString> expect{
            { "dolphin_overclock", "dolphin_overclock_enable" },
            { "dolphin_vi_overclock", "dolphin_vi_overclock_enable" },
        };
        int found = 0;
        for (const auto& d : schema_) {
            if (!expect.contains(d.key)) continue;
            ++found;
            QCOMPARE(d.dependsOn, expect.value(d.key));
        }
        QCOMPARE(found, 2);
    }

    void hubCards_referencedByEntries() {
        DolphinLibretroAdapter a;
        for (const auto& card : a.settingsHubCards()) {
            bool found = false;
            for (const auto& d : schema_) if (d.category == card.categoryKey) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("hub card '%1' (categoryKey='%2') matches no row")
                                           .arg(card.title).arg(card.categoryKey)));
        }
    }

    void previewSpec_osd_isOsd_elsewhereEmpty() {
        DolphinLibretroAdapter a;
        const auto osd = a.previewSpec("Graphics", "On-Screen Display");
        QCOMPARE(osd.previewType, QStringLiteral("osd"));
        QVERIFY(a.previewSpec("Graphics", "General").previewType.isEmpty());
    }

    void previewKeys_existInSchema() {
        DolphinLibretroAdapter a;
        QSet<QString> keys;
        for (const auto& d : schema_) keys.insert(d.key);
        const auto osd = a.previewSpec("Graphics", "On-Screen Display");
        for (auto it = osd.keyToProperty.constBegin(); it != osd.keyToProperty.constEnd(); ++it)
            QVERIFY2(keys.contains(it.key()),
                qPrintable(QString("previewSpec key '%1' is not a declared schema row").arg(it.key())));
    }
};

QTEST_GUILESS_MAIN(TestDolphinLibretroSchema)
#include "test_dolphin_libretro_schema.moc"
