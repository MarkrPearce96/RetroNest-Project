// cpp/tests/test_pcsx2_libretro_schema.cpp
//
// Shape guard for Pcsx2LibretroAdapter::settingsSchema() + settingsHubCards()
// + previewSpec() + controller schema. Replaces the SP8-deleted schema tests
// (their tools/test_core_options harness is also gone). Locks in schema
// structure and the current first-run defaults so any drift (renamed key,
// changed default, dropped sub-tab, out-of-sync Recommended mirror) trips
// loud rather than producing a silently-wrong UI or — worse — an
// OptionsStore::load whitelist mismatch that wipes persisted user settings.
//
// Cross-repo parity needs no checker since Packet 7 Stage 2: the schema
// renders FROM the core's declared option table (committed fixture below),
// so the two sides cannot drift. This test pins the RetroNest-side merge
// contract only.

#include <QtTest>
#include <QFileInfo>
#include <QSet>
#include "adapters/libretro/pcsx2_libretro_adapter.h"
#include "core/setting_def.h"
#include "core/binding_def.h"
#include "core/controller_type_def.h"
#include "core/libretro/input_router.h"

class TestPcsx2LibretroSchema : public QObject {
    Q_OBJECT
    Pcsx2LibretroAdapter adapter_;
    QVector<SettingDef> schema_;

    // First row carrying `key` (cross-listed keys are verified identical by
    // crossListedRows_shareValuesAndDefault, so "first" is representative).
    const SettingDef* row(const QString& key) const {
        for (const auto& d : schema_)
            if (d.key == key) return &d;
        return nullptr;
    }

    // Extracts the master keys referenced by a dependsOn expression, per the
    // setting_dependency.h grammar: terms joined by '&&' or '||' (never
    // both), each term one of  key | !key | key=value | key!=value.
    static QStringList masterKeysOf(const QString& expression) {
        QString expr = expression.trimmed();
        const bool hasAnd = expr.contains(QStringLiteral("&&"));
        const bool hasOr  = expr.contains(QStringLiteral("||"));
        if (hasAnd && hasOr)
            return {};  // caller asserts this case separately
        const QString sep = hasOr ? QStringLiteral("||") : QStringLiteral("&&");
        QStringList keys;
        for (QString term : expr.split(sep, Qt::SkipEmptyParts)) {
            term = term.trimmed();
            const int neIdx = term.indexOf(QStringLiteral("!="));
            if (neIdx >= 0) { keys << term.left(neIdx).trimmed(); continue; }
            const int eqIdx = term.indexOf(QChar('='));
            if (eqIdx >= 0) { keys << term.left(eqIdx).trimmed(); continue; }
            if (term.startsWith(QChar('!')))
                term = term.mid(1).trimmed();
            keys << term;
        }
        return keys;
    }

private slots:
    void initTestCase() {
        // Packet 7 Stage 2: the schema renders from the core's declared
        // option table — hermetic tests inject the committed fixture
        // (recorded at conversion time by the retired test_schema_parity tool) instead of
        // touching the live sidecar / prober.
        const QString fixture = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/fixtures/declared/pcsx2_declared_options.json";
        const auto doc = DeclaredOptionsDoc::load(fixture);
        QVERIFY2(doc.has_value(), "declared fixture missing");
        adapter_.setDeclaredDocForTest(*doc);
        schema_ = adapter_.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void totalCount_matchesOverlay() {
        // 89 curated core options (full coverage of the declared table) +
        // 15 Recommended cross-listings = 104 rows. A key the core stops
        // declaring is skipped by the merge with only a qWarning — this
        // count trips loud instead. New upstream options arrive hidden
        // (uncurated) and don't move this number until curated.
        QCOMPARE(schema_.size(), 104);
    }

    void everyRow_isPcsx2PrefixedLibretroCombo() {
        // The libretro adapter routes everything through the options.json
        // sidecar — no Ini or FrontendSetting rows today. Every key must
        // carry the pcsx2_ prefix (the core's option namespace) and be a
        // Combo (libretro core options v2 is combo-only).
        for (const auto& d : schema_) {
            QVERIFY2(d.storage == SettingDef::Storage::LibretroOption,
                qPrintable(QString("row '%1' is not LibretroOption storage").arg(d.key)));
            QVERIFY2(d.key.startsWith("pcsx2_"),
                qPrintable(QString("LibretroOption key '%1' missing pcsx2_ prefix").arg(d.key)));
            QVERIFY2(d.type == SettingDef::Combo,
                qPrintable(QString("row '%1' is not Combo type").arg(d.key)));
        }
    }

    void everyDefault_isInNonEmptyOptions() {
        for (const auto& d : schema_) {
            QVERIFY2(!d.options.isEmpty(),
                qPrintable(QString("row '%1' has an empty values list").arg(d.key)));
            bool found = false;
            for (const auto& opt : d.options)
                if (opt.second == d.defaultValue) { found = true; break; }
            QVERIFY2(found, qPrintable(QString("row '%1' default '%2' not in its options")
                                           .arg(d.key).arg(d.defaultValue)));
        }
    }

    void noDuplicateKeysPerCategory() {
        // Recommended (and Graphics/Display for renderer + aspect) deliberately
        // re-reference keys that live in other categories, so global
        // uniqueness does not hold — enforce uniqueness within each category.
        QSet<QString> seen;  // "category/key"
        for (const auto& d : schema_) {
            const QString id = d.category + "/" + d.key;
            QVERIFY2(!seen.contains(id),
                qPrintable(QString("duplicate key '%1' in category '%2'").arg(d.key).arg(d.category)));
            seen.insert(id);
        }
    }

    void crossListedRows_shareValuesAndDefault() {
        // Cross-listed rows (e.g. pcsx2_renderer on Recommended AND
        // Graphics > Display) back the same core option; if their value
        // lists or defaults drift apart, one view shows options the other
        // silently rejects. Pin: every duplicate of a key is byte-identical
        // in options + default.
        QHash<QString, const SettingDef*> first;
        for (const auto& d : schema_) {
            const auto it = first.constFind(d.key);
            if (it == first.constEnd()) { first.insert(d.key, &d); continue; }
            const SettingDef* o = it.value();
            QVERIFY2(d.defaultValue == o->defaultValue,
                qPrintable(QString("cross-listed row '%1' default drift: '%2' vs '%3'")
                               .arg(d.key, d.defaultValue, o->defaultValue)));
            QVERIFY2(d.options == o->options,
                qPrintable(QString("cross-listed row '%1' options drift between '%2' and '%3'")
                               .arg(d.key, d.category, o->category)));
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

    void graphicsSubTabs_matchStandaloneLayout() {
        QCOMPARE(adapter_.settingsCategoriesWithSubTabs(), QStringList{"Graphics"});
        QSet<QString> subs;
        for (const auto& d : schema_) {
            if (d.category == "Graphics") {
                subs.insert(d.subcategory);
            } else {
                QVERIFY2(d.subcategory.isEmpty(),
                    qPrintable(QString("non-Graphics row '%1' carries subcategory '%2'")
                                   .arg(d.key, d.subcategory)));
            }
        }
        const QSet<QString> expect{"Display", "Rendering", "Texture Replacement",
                                   "Post-Processing", "On-Screen Display"};
        QCOMPARE(subs, expect);
    }

    void rendererRows_pinLibretroBackendEnum() {
        // The libretro variant's renderer enum is Auto/Metal/Software/Null —
        // NOT the standalone Auto/OpenGL/Vulkan/Metal/Software set. Both the
        // Recommended row and its Graphics > Display mirror must pin it.
        const QVector<QPair<QString, QString>> expect{
            {"Auto", "auto"},
            {"Metal", "metal"},
            {"Software", "software"},
            {"Null", "null"},
        };
        int copies = 0;
        for (const auto& d : schema_) {
            if (d.key != "pcsx2_renderer") continue;
            ++copies;
            QCOMPARE(d.options, expect);
            QCOMPARE(d.defaultValue, QString("auto"));
        }
        QCOMPARE(copies, 2);  // Recommended + Graphics > Display
    }

    void internalResolution_pins1xTo12x() {
        const SettingDef* d = row("pcsx2_upscale_multiplier");
        QVERIFY2(d, "pcsx2_upscale_multiplier not found in schema");
        QCOMPARE(d->defaultValue, QString("2"));  // 2x ≈ 720p first-run default
        QCOMPARE(d->options.size(), 12);
        for (int i = 0; i < d->options.size(); ++i)
            QCOMPARE(d->options[i].second, QString::number(i + 1));  // "1".."12"
    }

    void firstRunDefaults_pinned() {
        auto def = [&](const QString& key) {
            const SettingDef* d = row(key);
            return d ? d->defaultValue : QString("<missing>");
        };
        QCOMPARE(def("pcsx2_aspect_ratio"), QString("16:9"));
        QCOMPARE(def("pcsx2_mtvu"), QString("enabled"));
        QCOMPARE(def("pcsx2_audio_sync_mode"), QString("TimeStretch"));
        QCOMPARE(def("pcsx2_audio_volume"), QString("100"));
        QCOMPARE(def("pcsx2_ee_cycle_rate"), QString("0"));
        QCOMPARE(def("pcsx2_normal_speed"), QString("1"));
        QCOMPARE(def("pcsx2_mc_slot1_enable"), QString("enabled"));
        QCOMPARE(def("pcsx2_mc_slot2_enable"), QString("enabled"));

        // Aspect values must match the INI strings verbatim (core parses
        // them byte-for-byte; "Auto 4:3/3:2" is the odd one out).
        QSet<QString> aspectVals;
        for (const auto& p : row("pcsx2_aspect_ratio")->options) aspectVals.insert(p.second);
        QCOMPARE(aspectVals, QSet<QString>({"Auto 4:3/3:2", "4:3", "16:9", "10:7", "Stretch"}));
    }

    void biosRows_fastBootChain() {
        // BIOS-adjacent rows: Fast Boot skips the Sony intro (default on);
        // Fast-Forward Through BIOS is its dependent (default off, gated on
        // pcsx2_fast_boot so it greys when Fast Boot is disabled).
        const SettingDef* fb = row("pcsx2_fast_boot");
        QVERIFY2(fb, "pcsx2_fast_boot not found in schema");
        QCOMPARE(fb->defaultValue, QString("enabled"));

        const SettingDef* ff = row("pcsx2_fast_boot_ff");
        QVERIFY2(ff, "pcsx2_fast_boot_ff not found in schema");
        QCOMPARE(ff->defaultValue, QString("disabled"));
        QCOMPARE(ff->dependsOn, QString("pcsx2_fast_boot"));
    }

    void dependsOnExpressions_parseAndResolveWithinCategory() {
        // Two invariants per setting_dependency.h + the refreshDependencies
        // findChildren limitation (masters are only visible on the same
        // page): no '&&'/'||' mixing, and every referenced master key must
        // have a row in the SAME category as the dependent — a cross-
        // category-only master silently never greys its dependents.
        QHash<QString, QSet<QString>> keysByCategory;
        for (const auto& d : schema_)
            keysByCategory[d.category].insert(d.key);
        for (const auto& d : schema_) {
            if (d.dependsOn.isEmpty()) continue;
            QVERIFY2(!(d.dependsOn.contains("&&") && d.dependsOn.contains("||")),
                qPrintable(QString("row '%1' dependsOn mixes '&&' and '||': %2")
                               .arg(d.key, d.dependsOn)));
            const QStringList masters = masterKeysOf(d.dependsOn);
            QVERIFY2(!masters.isEmpty(),
                qPrintable(QString("row '%1' dependsOn '%2' yields no master keys")
                               .arg(d.key, d.dependsOn)));
            for (const QString& m : masters)
                QVERIFY2(keysByCategory.value(d.category).contains(m),
                    qPrintable(QString("row '%1' (category '%2') dependsOn master '%3' "
                                       "has no row in the same category")
                                   .arg(d.key, d.category, m)));
        }
    }

    void hubCards_bijectWithSchemaCategories() {
        const auto cards = adapter_.settingsHubCards();
        QCOMPARE(cards.size(), 5);  // Recommended, Emulation, Graphics, Audio, Memory Cards
        QSet<QString> schemaCategories;
        for (const auto& d : schema_) schemaCategories.insert(d.category);
        QSet<QString> cardCategories;
        for (const auto& card : cards) {
            QVERIFY2(schemaCategories.contains(card.categoryKey),
                qPrintable(QString("hub card '%1' (categoryKey='%2') matches no row")
                               .arg(card.title).arg(card.categoryKey)));
            cardCategories.insert(card.categoryKey);
        }
        // Reverse direction: every schema category must be reachable from a
        // hub card, or its rows are dead weight the user can never open.
        QCOMPARE(cardCategories, schemaCategories);
    }

    void previewSpec_aspectAndOsd_wired() {
        const auto aspect = adapter_.previewSpec("Recommended", "");
        QCOMPARE(aspect.previewType, QStringLiteral("aspect"));
        const auto osd = adapter_.previewSpec("Graphics", "On-Screen Display");
        QCOMPARE(osd.previewType, QStringLiteral("osd"));
        QVERIFY(adapter_.previewSpec("Graphics", "Display").previewType.isEmpty());
        QVERIFY(adapter_.previewSpec("Audio", "").previewType.isEmpty());

        // Every preview-bound key must be a declared schema row, or
        // GenericSettingsPage::wirePreviewBinding never fires for it.
        QSet<QString> keys;
        for (const auto& d : schema_) keys.insert(d.key);
        for (const auto& spec : {aspect, osd})
            for (auto it = spec.keyToProperty.constBegin(); it != spec.keyToProperty.constEnd(); ++it)
                QVERIFY2(keys.contains(it.key()),
                    qPrintable(QString("previewSpec key '%1' is not a declared schema row").arg(it.key())));
    }

    void controllerSchema_singleDualShock2() {
        const auto types = adapter_.controllerTypes();
        QCOMPARE(types.size(), 1);
        QCOMPARE(types[0].id, QString("DualShock2"));
        QVERIFY2(!types[0].svgResource.isEmpty(), "missing svgResource for DualShock2");

        // Shape, not every row: 16 digital bindings (D-Pad×4, face×4,
        // shoulders/triggers×4, stick clicks×2, Start/Select), every key
        // resolving to a RetroPad slot (game_session skips
        // RetroPadSlot::None, so an unresolved key is silently dead) and a
        // valid cardSlot, seeded with SDL-0/... defaults. The empty-type
        // call (ensureConfig + game_session path) must return the same set.
        static const QSet<QString> validSlots{
            "DPad", "FaceButtons", "LeftAnalog", "RightAnalog", "Shoulders", "System",
        };
        const auto bindings = adapter_.controllerBindingDefsForType("DualShock2");
        QCOMPARE(bindings.size(), 16);
        for (const auto& b : bindings) {
            QCOMPARE(b.kind, BindingDef::Button);
            QVERIFY2(retroPadSlotFromKey(b.key) != RetroPadSlot::None,
                qPrintable(QString("unresolved digital key '%1' (%2)").arg(b.key, b.label)));
            QVERIFY2(validSlots.contains(b.cardSlot),
                qPrintable(QString("binding '%1' has invalid cardSlot '%2'").arg(b.label, b.cardSlot)));
            QVERIFY2(b.defaultValue.startsWith("SDL-0/"),
                qPrintable(QString("binding '%1' default '%2' not SDL-0/ form").arg(b.label, b.defaultValue)));
        }
        QCOMPARE(adapter_.controllerBindingDefsForType({}).size(), bindings.size());
    }

    void maxLibretroPlayers_isOne() {
        // No override in the adapter today — PCSX2 libretro exposes a single
        // RetroPad port (multitap memory-card slots exist, but not multi-pad
        // input). If multi-player lands, update this pin deliberately.
        QCOMPARE(adapter_.maxLibretroPlayers(), 1);
    }
};

QTEST_GUILESS_MAIN(TestPcsx2LibretroSchema)
#include "test_pcsx2_libretro_schema.moc"
