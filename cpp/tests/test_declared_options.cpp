// Packet 7 Stage 2: DeclaredOptionsDoc — full-metadata capture of a core's
// SET_CORE_OPTIONS_V2 table + JSON sidecar round-trip.
#include <QtTest>
#include <QTemporaryDir>
#include "libretro.h"
#include "core/libretro/declared_options.h"

namespace {

// Fixture: two options (one categorized with value labels, one bare) + one category.
const retro_core_option_v2_definition kDefs[] = {
    {
        "fake_speed",                       // key
        "Emulation Speed",                  // desc
        nullptr,                            // desc_categorized
        "How fast the fake core pretends to run.",  // info
        nullptr,                            // info_categorized
        "perf",                             // category_key
        { { "1", "Normal" }, { "2", "Double" }, { nullptr, nullptr } },
        "1"                                 // default_value
    },
    {
        "fake_bool",
        "Fake Toggle",
        nullptr,
        nullptr,                            // no info
        nullptr,
        nullptr,                            // no category
        { { "enabled", nullptr }, { "disabled", nullptr }, { nullptr, nullptr } },
        "disabled"
    },
    { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, {{nullptr, nullptr}}, nullptr }
};

const retro_core_option_v2_category kCats[] = {
    { "perf", "Performance", "Speed-related options." },
    { nullptr, nullptr, nullptr }
};

retro_core_options_v2 fixtureV2()
{
    retro_core_options_v2 v2{};
    v2.categories = const_cast<retro_core_option_v2_category*>(kCats);
    v2.definitions = const_cast<retro_core_option_v2_definition*>(kDefs);
    return v2;
}

} // namespace

class TestDeclaredOptions : public QObject {
    Q_OBJECT
private slots:
    void v2Populate() {
        DeclaredOptionsDoc doc;
        const auto v2 = fixtureV2();
        populateFromV2(doc, &v2);

        QCOMPARE(doc.options.size(), 2);
        QCOMPARE(doc.categories.size(), 1);
        QCOMPARE(doc.categories[0].key, QString("perf"));
        QCOMPARE(doc.categories[0].label, QString("Performance"));
        QCOMPARE(doc.categories[0].info, QString("Speed-related options."));

        const auto& speed = doc.options[0];
        QCOMPARE(speed.key, QString("fake_speed"));
        QCOMPARE(speed.label, QString("Emulation Speed"));
        QCOMPARE(speed.categoryKey, QString("perf"));
        QCOMPARE(speed.info, QString("How fast the fake core pretends to run."));
        QCOMPARE(speed.defaultValue, QString("1"));
        QCOMPARE(speed.values.size(), 2);
        QCOMPARE(speed.values[0].value, QString("1"));
        QCOMPARE(speed.values[0].label, QString("Normal"));
        QCOMPARE(speed.values[1].value, QString("2"));
        QCOMPARE(speed.values[1].label, QString("Double"));

        const auto& b = doc.options[1];
        QCOMPARE(b.key, QString("fake_bool"));
        QVERIFY(b.categoryKey.isEmpty());
        QVERIFY(b.info.isEmpty());
        QCOMPARE(b.values.size(), 2);
        QVERIFY(b.values[0].label.isEmpty());   // no label declared
        QCOMPARE(b.defaultValue, QString("disabled"));
    }

    void jsonRoundTrip() {
        DeclaredOptionsDoc doc;
        const auto v2 = fixtureV2();
        populateFromV2(doc, &v2);
        doc.coreLibraryVersion = "fake-1.0";

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath("declared_options.json");
        QVERIFY(doc.save(path));

        const auto loaded = DeclaredOptionsDoc::load(path);
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->format, 1);
        QCOMPARE(loaded->coreLibraryVersion, QString("fake-1.0"));
        QCOMPARE(loaded->categories.size(), doc.categories.size());
        QCOMPARE(loaded->options.size(), doc.options.size());
        for (int i = 0; i < doc.options.size(); ++i) {
            QCOMPARE(loaded->options[i].key, doc.options[i].key);
            QCOMPARE(loaded->options[i].label, doc.options[i].label);
            QCOMPARE(loaded->options[i].categoryKey, doc.options[i].categoryKey);
            QCOMPARE(loaded->options[i].info, doc.options[i].info);
            QCOMPARE(loaded->options[i].defaultValue, doc.options[i].defaultValue);
            QCOMPARE(loaded->options[i].values.size(), doc.options[i].values.size());
            for (int j = 0; j < doc.options[i].values.size(); ++j) {
                QCOMPARE(loaded->options[i].values[j].value, doc.options[i].values[j].value);
                QCOMPARE(loaded->options[i].values[j].label, doc.options[i].values[j].label);
            }
        }
    }

    void thinView() {
        DeclaredOptionsDoc doc;
        const auto v2 = fixtureV2();
        populateFromV2(doc, &v2);

        const QVector<CoreOption> thin = doc.toCoreOptions();
        QCOMPARE(thin.size(), 2);
        QCOMPARE(thin[0].key, QString("fake_speed"));
        QCOMPARE(thin[0].label, QString("Emulation Speed"));
        QCOMPARE(thin[0].defaultValue, QString("1"));
        QCOMPARE(thin[0].values, (QStringList{ "1", "2" }));
        QCOMPARE(thin[1].values, (QStringList{ "enabled", "disabled" }));
    }

    void loadMissingFile() {
        const auto loaded = DeclaredOptionsDoc::load("/nonexistent/path/declared_options.json");
        QVERIFY(!loaded.has_value());
    }
};

QTEST_APPLESS_MAIN(TestDeclaredOptions)
#include "test_declared_options.moc"
