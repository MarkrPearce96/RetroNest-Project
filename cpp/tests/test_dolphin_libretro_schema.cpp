// cpp/tests/test_dolphin_libretro_schema.cpp
//
// SP6 shape guard for DolphinLibretroAdapter::settingsSchema() +
// settingsHubCards() + previewSpec(). Asserts contracts that prevent
// silent breakage; cross-repo value/default parity is enforced separately
// by dolphin-libretro/tools/check_schema_fidelity.py.

#include <QtTest>
#include <QSet>
#include "adapters/libretro/dolphin_libretro_adapter.h"
#include "core/setting_def.h"

class TestDolphinLibretroSchema : public QObject {
    Q_OBJECT
private slots:
    void everyLibretroKey_hasDolphinPrefix() {
        DolphinLibretroAdapter a;
        for (const auto& d : a.settingsSchema())
            if (d.storage == SettingDef::Storage::LibretroOption)
                QVERIFY2(d.key.startsWith("dolphin_"),
                    qPrintable(QString("LibretroOption key '%1' missing dolphin_ prefix").arg(d.key)));
    }

    void everyRow_isLibretroCombo() {
        DolphinLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(d.storage == SettingDef::Storage::LibretroOption,
                qPrintable(QString("row '%1' is not LibretroOption storage").arg(d.key)));
            QVERIFY2(d.type == SettingDef::Combo,
                qPrintable(QString("row '%1' is not Combo type").arg(d.key)));
        }
    }

    void everyDefault_isInOptions() {
        DolphinLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            bool found = false;
            for (const auto& opt : d.options)
                if (opt.second == d.defaultValue) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("row '%1' default '%2' not in its options")
                                           .arg(d.key).arg(d.defaultValue)));
        }
    }

    void allFiveSubTabs_present() {
        DolphinLibretroAdapter a;
        QSet<QString> subs;
        for (const auto& d : a.settingsSchema())
            if (d.category == "Graphics") subs.insert(d.subcategory);
        const QSet<QString> expect{"General","Enhancements","Hacks","Advanced","On-Screen Display"};
        QCOMPARE(subs, expect);
    }

    void noDuplicateKeysPerCategory() {
        // Recommended deliberately re-references keys that live in other
        // categories, so global uniqueness no longer holds — enforce
        // uniqueness within each category instead.
        DolphinLibretroAdapter a;
        QSet<QString> seen;  // "category/key"
        for (const auto& d : a.settingsSchema()) {
            const QString id = d.category + "/" + d.key;
            QVERIFY2(!seen.contains(id),
                qPrintable(QString("duplicate key '%1' in category '%2'").arg(d.key).arg(d.category)));
            seen.insert(id);
        }
    }

    void recommendedRows_haveAHomeElsewhere() {
        DolphinLibretroAdapter a;
        QSet<QString> nonRecKeys;
        for (const auto& d : a.settingsSchema())
            if (d.category != "Recommended") nonRecKeys.insert(d.key);
        for (const auto& d : a.settingsSchema())
            if (d.category == "Recommended")
                QVERIFY2(nonRecKeys.contains(d.key),
                    qPrintable(QString("Recommended row '%1' has no home row in another category").arg(d.key)));
    }

    void hubCards_referencedByEntries() {
        DolphinLibretroAdapter a;
        const auto schema = a.settingsSchema();
        for (const auto& card : a.settingsHubCards()) {
            bool found = false;
            for (const auto& d : schema) if (d.category == card.categoryKey) { found = true; break; }
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
        for (const auto& d : a.settingsSchema()) keys.insert(d.key);
        const auto osd = a.previewSpec("Graphics", "On-Screen Display");
        for (auto it = osd.keyToProperty.constBegin(); it != osd.keyToProperty.constEnd(); ++it)
            QVERIFY2(keys.contains(it.key()),
                qPrintable(QString("previewSpec key '%1' is not a declared schema row").arg(it.key())));
    }
};

QTEST_GUILESS_MAIN(TestDolphinLibretroSchema)
#include "test_dolphin_libretro_schema.moc"
