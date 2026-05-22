// cpp/tests/test_ppsspp_libretro_schema.cpp
//
// Phase B+C regression guard for PpssppLibretroAdapter::settingsSchema()
// and settingsHubCards(). Asserts data-shape contracts that prevent
// silent breakage if upstream renames an option or the schema drifts.

#include <QtTest>
#include <QHash>
#include <QSet>
#include "adapters/libretro/ppsspp_libretro_adapter.h"
#include "core/setting_def.h"

class TestPpssppLibretroSchema : public QObject {
    Q_OBJECT
private:
    // The 43 unique upstream option keys this schema exposes.
    // Maintained in sync with libretro/libretro_core_options.h.
    // Changes here must be matched by the .cpp implementation.
    static QSet<QString> knownUpstreamKeys() {
        return {
            // System (11)
            "ppsspp_cpu_core", "ppsspp_fast_memory",
            "ppsspp_ignore_bad_memory_access", "ppsspp_io_timing_method",
            "ppsspp_force_lag_sync", "ppsspp_locked_cpu_speed",
            "ppsspp_memstick_inserted", "ppsspp_cache_iso",
            "ppsspp_cheats", "ppsspp_language", "ppsspp_psp_model",
            // Video (22)
            "ppsspp_backend", "ppsspp_software_rendering",
            "ppsspp_internal_resolution", "ppsspp_mulitsample_level",
            "ppsspp_cropto16x9", "ppsspp_frameskip", "ppsspp_frameskiptype",
            "ppsspp_auto_frameskip", "ppsspp_frame_duplication",
            "ppsspp_detect_vsync_swap_interval", "ppsspp_inflight_frames",
            "ppsspp_gpu_hardware_transform", "ppsspp_software_skinning",
            "ppsspp_hardware_tesselation", "ppsspp_texture_scaling_type",
            "ppsspp_texture_scaling_level", "ppsspp_texture_deposterize",
            "ppsspp_texture_shader", "ppsspp_texture_anisotropic_filtering",
            "ppsspp_texture_filtering", "ppsspp_smart_2d_texture_filtering",
            "ppsspp_texture_replacement",
            // Input (4)
            "ppsspp_button_preference", "ppsspp_analog_is_circular",
            "ppsspp_analog_deadzone", "ppsspp_analog_sensitivity",
            // Hacks (6)
            "ppsspp_skip_buffer_effects", "ppsspp_disable_range_culling",
            "ppsspp_skip_gpu_readbacks", "ppsspp_lazy_texture_caching",
            "ppsspp_spline_quality", "ppsspp_lower_resolution_for_effects",
        };
    }

    // FrontendSetting keys — RetroNest-side concerns, not libretro core
    // options. These intentionally drop the `ppsspp_` prefix because they
    // live in frontend.json shared across adapters (see mgba's pattern).
    static QSet<QString> knownFrontendKeys() {
        return { "aspect_mode", "integer_scale" };
    }

private slots:
    void totalCount_matchesSpec() {
        PpssppLibretroAdapter a;
        // 43 libretro originals + 10 Recommended libretro dupes
        //  + 2 FrontendSetting rows in Recommended (aspect_mode, integer_scale) = 55.
        QCOMPARE(a.settingsSchema().size(), 55);
    }

    void everyKey_hasValidShape() {
        // LibretroOption keys must carry the ppsspp_ prefix; FrontendSetting
        // keys (aspect_mode / integer_scale) intentionally don't because
        // they're shared across adapters.
        PpssppLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            if (d.storage == SettingDef::Storage::LibretroOption) {
                QVERIFY2(d.key.startsWith("ppsspp_"),
                         qPrintable(QString("LibretroOption key '%1' missing ppsspp_ prefix").arg(d.key)));
            }
        }
    }

    void everyKey_isKnown() {
        PpssppLibretroAdapter a;
        const auto libretro = knownUpstreamKeys();
        const auto fe = knownFrontendKeys();
        for (const auto& d : a.settingsSchema()) {
            const auto& allow =
                (d.storage == SettingDef::Storage::FrontendSetting) ? fe : libretro;
            QVERIFY2(allow.contains(d.key),
                     qPrintable(QString("SettingDef key '%1' (storage=%2) is not in the known allow-list "
                                        "(this catches stale or renamed options)")
                                    .arg(d.key)
                                    .arg(d.storage == SettingDef::Storage::FrontendSetting ? "Frontend" : "Libretro")));
        }
    }

    void everyDefault_isInOptions() {
        PpssppLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            QVERIFY2(d.type == SettingDef::Combo,
                     qPrintable(QString("SettingDef '%1' is not Combo type (this phase only ships Combos)").arg(d.key)));
            bool found = false;
            for (const auto& opt : d.options) {
                if (opt.second == d.defaultValue) { found = true; break; }
            }
            QVERIFY2(found,
                     qPrintable(QString("SettingDef '%1' defaultValue '%2' not present in its options list")
                                    .arg(d.key).arg(d.defaultValue)));
        }
    }

    void recommendedHasNaturalDupe() {
        // Libretro-backed Recommended rows must have a System/Video/Input/Hacks
        // twin so users editing in either place mutate the same OptionsStore
        // key. FrontendSetting rows on Recommended (aspect_mode etc.) are
        // intentionally NOT duplicated — they live only in Recommended.
        PpssppLibretroAdapter a;
        const auto schema = a.settingsSchema();
        const QSet<QString> naturalCats{"System", "Video", "Input", "Hacks"};
        for (const auto& rec : schema) {
            if (rec.category != "Recommended") continue;
            if (rec.storage != SettingDef::Storage::LibretroOption) continue;
            bool foundDupe = false;
            for (const auto& nat : schema) {
                if (nat.key == rec.key && nat.category != "Recommended"
                    && naturalCats.contains(nat.category)) {
                    foundDupe = true;
                    break;
                }
            }
            QVERIFY2(foundDupe,
                     qPrintable(QString("Recommended libretro setting '%1' has no matching entry "
                                        "under System/Video/Input/Hacks").arg(rec.key)));
        }
    }

    void hubCards_referencedByEntries() {
        PpssppLibretroAdapter a;
        const auto cards = a.settingsHubCards();
        const auto schema = a.settingsSchema();
        for (const auto& card : cards) {
            bool found = false;
            for (const auto& d : schema) {
                if (d.category == card.categoryKey) { found = true; break; }
            }
            QVERIFY2(found,
                     qPrintable(QString("Hub card '%1' (categoryKey='%2') has no matching SettingDef entries")
                                    .arg(card.title).arg(card.categoryKey)));
        }
    }

    void allEntries_useExpectedStorage() {
        // Phase B/C: every entry is either LibretroOption (the 43+10 core
        // options) or FrontendSetting (the 2 RetroNest-side aspect rows).
        PpssppLibretroAdapter a;
        for (const auto& d : a.settingsSchema()) {
            const bool ok = d.storage == SettingDef::Storage::LibretroOption
                         || d.storage == SettingDef::Storage::FrontendSetting;
            QVERIFY2(ok,
                     qPrintable(QString("SettingDef '%1' uses an unexpected storage type "
                                        "(must be LibretroOption or FrontendSetting)").arg(d.key)));
        }
    }

    void previewSpec_recommended_isAspect() {
        // The Recommended card hosts the AspectRatioPreview pane, bound to
        // aspect_mode. Other (category, subcategory) pairs return empty.
        PpssppLibretroAdapter a;
        const auto spec = a.previewSpec("Recommended", "");
        QCOMPARE(spec.previewType, QStringLiteral("aspect"));
        QVERIFY(spec.keyToProperty.contains("aspect_mode"));
        QCOMPARE(spec.keyToProperty.value("aspect_mode"), QStringLiteral("aspectMode"));

        // Sanity: other pages get no preview.
        QVERIFY(a.previewSpec("System", "").previewType.isEmpty());
        QVERIFY(a.previewSpec("Video",  "").previewType.isEmpty());
        QVERIFY(a.previewSpec("Hacks",  "").previewType.isEmpty());
    }

    void frontendSettingDefaults_includeAspectRows() {
        // aspect_mode + integer_scale must be seeded into frontend.json so
        // FrontendSettingsStore::get() returns sane defaults before the
        // user touches anything.
        PpssppLibretroAdapter a;
        const auto defs = a.frontendSettingDefaults();
        QHash<QString, QString> byKey;
        for (const auto& p : defs) byKey.insert(p.first, p.second);
        QCOMPARE(byKey.value("aspect_mode"),   QStringLiteral("native"));
        QCOMPARE(byKey.value("integer_scale"), QStringLiteral("OFF"));
    }
};

QTEST_GUILESS_MAIN(TestPpssppLibretroSchema)
#include "test_ppsspp_libretro_schema.moc"
