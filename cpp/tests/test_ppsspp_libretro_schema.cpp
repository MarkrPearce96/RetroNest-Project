// cpp/tests/test_ppsspp_libretro_schema.cpp
//
// Shape guard for PpssppLibretroAdapter's settings schema and
// settingsHubCards(). Since Packet 7 Stage 2 the schema renders from the
// core's declared option table (committed fixture) × the adapter's
// curation overlay — this guard locks the merged shape: routing, the
// Recommended cross-listings, storage kinds, and the deliberate RetroNest
// default overrides.

#include <QtTest>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QTemporaryDir>
#include "adapters/libretro/ppsspp_libretro_adapter.h"
#include "core/paths.h"
#include "core/setting_def.h"

// Minimal concrete LibretroAdapter used to probe base-class defaults
// (coreId() is the only pure virtual on the libretro base).
class StubLibretroAdapter : public LibretroAdapter {
public:
    QString coreId() const override { return QStringLiteral("stub"); }
};

class TestPpssppLibretroSchema : public QObject {
    Q_OBJECT
    QVector<SettingDef> schema_;
private:
    // The 43 upstream option keys the overlay curates. The core declares 76
    // (fixture) — the other 33 (ad-hoc networking, MAC digits, UPnP, …) are
    // deliberately uncurated: valid in OptionsStore, hidden from the UI.
    static QSet<QString> curatedUpstreamKeys() {
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
        // Only Integer Scale — PPSSPP has no aspect setting (it renders at the
        // game's native aspect; forcing one would distort widescreen titles).
        return { "integer_scale" };
    }

private slots:
    void initTestCase() {
        PpssppLibretroAdapter adapter;
        // Packet 7 Stage 2: the schema renders from the core's declared
        // option table — hermetic tests inject the committed fixture
        // (recorded at conversion time by the retired test_schema_parity tool) instead of
        // touching the live sidecar / prober.
        const QString fixture = QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()
            + "/fixtures/declared/ppsspp_declared_options.json";
        const auto doc = DeclaredOptionsDoc::load(fixture);
        QVERIFY2(doc.has_value(), "declared fixture missing");
        adapter.setDeclaredDocForTest(*doc);
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void totalCount_matchesOverlay() {
        // 43 curated originals + 10 Recommended cross-listings
        //  + 1 FrontendSetting row in Recommended (integer_scale) = 54.
        //  (aspect_mode was removed — PPSSPP has no aspect setting.)
        QCOMPARE(schema_.size(), 54);
    }

    void everyKey_hasValidShape() {
        // LibretroOption keys must carry the ppsspp_ prefix; FrontendSetting
        // keys (aspect_mode / integer_scale) intentionally don't because
        // they're shared across adapters.
        for (const auto& d : schema_) {
            if (d.storage == SettingDef::Storage::LibretroOption) {
                QVERIFY2(d.key.startsWith("ppsspp_"),
                         qPrintable(QString("LibretroOption key '%1' missing ppsspp_ prefix").arg(d.key)));
            }
        }
    }

    void everyKey_isKnown() {
        const auto libretro = curatedUpstreamKeys();
        const auto fe = knownFrontendKeys();
        for (const auto& d : schema_) {
            const auto& allow =
                (d.storage == SettingDef::Storage::FrontendSetting) ? fe : libretro;
            QVERIFY2(allow.contains(d.key),
                     qPrintable(QString("SettingDef key '%1' (storage=%2) is not in the known allow-list "
                                        "(this catches stale or renamed options)")
                                    .arg(d.key)
                                    .arg(d.storage == SettingDef::Storage::FrontendSetting ? "Frontend" : "Libretro")));
        }
    }

    void everyCuratedKey_isRendered() {
        // The inverse of everyKey_isKnown: an overlay key the core no longer
        // declares is skipped by the merge with only a qWarning — this trips
        // loud instead (the mGBA pilot found 4 such dead rows).
        QSet<QString> rendered;
        for (const auto& d : schema_) {
            if (d.storage == SettingDef::Storage::LibretroOption)
                rendered.insert(d.key);
        }
        for (const auto& k : curatedUpstreamKeys()) {
            QVERIFY2(rendered.contains(k),
                     qPrintable(QString("curated key '%1' missing from merged schema — "
                                        "core no longer declares it?").arg(k)));
        }
    }

    void everyDefault_isInOptions() {
        for (const auto& d : schema_) {
            QVERIFY2(d.type == SettingDef::Combo,
                     qPrintable(QString("SettingDef '%1' is not Combo type (this schema only ships Combos)").arg(d.key)));
            bool found = false;
            for (const auto& opt : d.options) {
                if (opt.second == d.defaultValue) { found = true; break; }
            }
            QVERIFY2(found,
                     qPrintable(QString("SettingDef '%1' defaultValue '%2' not present in its options list")
                                    .arg(d.key).arg(d.defaultValue)));
        }
    }

    void intentionalDefaultOverrides_hold() {
        // The one deliberate RetroNest default: render at 2x, not the
        // core's native 1x. Everything else adopts the core's default by
        // construction (empty defaultOverride in the overlay).
        bool found = false;
        for (const auto& d : schema_) {
            if (d.key != "ppsspp_internal_resolution") continue;
            found = true;
            QCOMPARE(d.defaultValue, QStringLiteral("960x544"));
        }
        QVERIFY2(found, "ppsspp_internal_resolution not found in schema");
    }

    void recommendedHasNaturalDupe() {
        // Libretro-backed Recommended rows must have a System/Video/Input/Hacks
        // twin so users editing in either place mutate the same OptionsStore
        // key. FrontendSetting rows on Recommended (aspect_mode etc.) are
        // intentionally NOT duplicated — they live only in Recommended.
        const QSet<QString> naturalCats{"System", "Video", "Input", "Hacks"};
        for (const auto& rec : schema_) {
            if (rec.category != "Recommended") continue;
            if (rec.storage != SettingDef::Storage::LibretroOption) continue;
            bool foundDupe = false;
            for (const auto& nat : schema_) {
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
        for (const auto& card : cards) {
            bool found = false;
            for (const auto& d : schema_) {
                if (d.category == card.categoryKey) { found = true; break; }
            }
            QVERIFY2(found,
                     qPrintable(QString("Hub card '%1' (categoryKey='%2') has no matching SettingDef entries")
                                    .arg(card.title).arg(card.categoryKey)));
        }
    }

    void allEntries_useExpectedStorage() {
        // Every entry is either LibretroOption (the 43+10 core options) or
        // FrontendSetting (the 2 RetroNest-side aspect rows).
        for (const auto& d : schema_) {
            const bool ok = d.storage == SettingDef::Storage::LibretroOption
                         || d.storage == SettingDef::Storage::FrontendSetting;
            QVERIFY2(ok,
                     qPrintable(QString("SettingDef '%1' uses an unexpected storage type "
                                        "(must be LibretroOption or FrontendSetting)").arg(d.key)));
        }
    }

    void duplicatedRows_haveConsistentDefaults() {
        // key → (category-of-first-seen, defaultValue-of-first-seen)
        QHash<QString, QPair<QString, QString>> firstSeen;
        QStringList violations;
        for (const auto& s : schema_) {
            if (s.storage != SettingDef::Storage::LibretroOption)
                continue;
            auto it = firstSeen.constFind(s.key);
            if (it == firstSeen.constEnd()) {
                firstSeen.insert(s.key, qMakePair(s.category, s.defaultValue));
            } else if (it.value().second != s.defaultValue) {
                violations << QString("'%1': %2=%3 vs %4=%5")
                                  .arg(s.key)
                                  .arg(it.value().first)
                                  .arg(it.value().second)
                                  .arg(s.category)
                                  .arg(s.defaultValue);
            }
        }
        if (!violations.isEmpty()) {
            QFAIL(qPrintable(QString("schema rows with the same key carry different "
                                     "defaultValue. Recommended card duplicates must "
                                     "match their canonical row. Violations: %1")
                                .arg(violations.join("; "))));
        }
    }

    void previewSpec_hasNoAspectPreview() {
        // PPSSPP has no aspect setting, so there's no AspectRatioPreview pane
        // anywhere — every (category, subcategory) returns an empty spec.
        PpssppLibretroAdapter a;
        QVERIFY(a.previewSpec("Recommended", "").previewType.isEmpty());
        QVERIFY(a.previewSpec("System", "").previewType.isEmpty());
        QVERIFY(a.previewSpec("Video",  "").previewType.isEmpty());
        QVERIFY(a.previewSpec("Hacks",  "").previewType.isEmpty());
    }

    void frontendSettingDefaults_integerScaleOnly() {
        // Only integer_scale is seeded into frontend.json; aspect_mode was
        // removed (no aspect setting for PPSSPP).
        PpssppLibretroAdapter a;
        const auto defs = a.frontendSettingDefaults();
        QHash<QString, QString> byKey;
        for (const auto& p : defs) byKey.insert(p.first, p.second);
        QVERIFY2(!byKey.contains("aspect_mode"), "PPSSPP should have no aspect_mode default");
        QCOMPARE(byKey.value("integer_scale"), QStringLiteral("OFF"));
    }

    // --- systemDirOverride (asset-directory handoff) --------------------
    // The PPSSPP core resolves its bundled assets at <system_dir>/PPSSPP/.
    // GameSession uses the adapter's systemDirOverride() when non-empty,
    // else falls back to Paths::biosDir(). Paths is a static-root holder,
    // so tests can redirect it at a QTemporaryDir via Paths::setRoot().

    void baseAdapter_systemDirOverride_isEmpty() {
        StubLibretroAdapter a;
        QVERIFY(a.systemDirOverride().isEmpty());
    }

    void ppssppOverride_returnsResourcesDir_whenPresent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(Paths::setRoot(tmp.path()));
        const QString res =
            tmp.path() + "/emulators/libretro/cores/ppsspp_libretro_resources";
        QVERIFY(QDir().mkpath(res));

        PpssppLibretroAdapter a;
        QCOMPARE(a.systemDirOverride(), res);
    }

    void ppssppOverride_isEmpty_whenResourcesDirAbsent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        QVERIFY(Paths::setRoot(tmp.path()));
        // No cores/ppsspp_libretro_resources under this root → the legacy
        // hand-copied {root}/bios/PPSSPP layout must keep working, which
        // requires an empty override (GameSession then uses biosDir()).
        PpssppLibretroAdapter a;
        QVERIFY(a.systemDirOverride().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPpssppLibretroSchema)
#include "test_ppsspp_libretro_schema.moc"
