#include <QtTest>
#include <QSet>
#include "adapters/ppsspp_adapter.h"
#include "core/setting_def.h"

// Tests that PPSSPPAdapter::settingsSchema() mirrors upstream PPSSPP's
// GameSettingsScreen panes verbatim — same top-level tabs, same group order,
// same setting order, same labels, same gating chains.
//
// Reference: references/ppsspp-master/UI/GameSettingsScreen.cpp.
//
// The per-category full-catalog tests below assert the exact set of INI keys
// that should appear in each category. Any addition / removal trips the test
// — that's the bug-net described in dolphin-schema-alignment.md.

class TestPPSSPPSchema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

    QStringList keysIn(const QString& category) const {
        QStringList keys;
        for (const auto& d : schema_)
            if (d.category == category) keys.append(d.key);
        return keys;
    }

    QStringList keysIn(const QString& category, const QString& group) const {
        QStringList keys;
        for (const auto& d : schema_)
            if (d.category == category && d.group == group) keys.append(d.key);
        return keys;
    }

    QStringList keysIn(const QString& category, const QString& subcategory,
                       const QString& group) const {
        QStringList keys;
        for (const auto& d : schema_) {
            if (d.category == category && d.subcategory == subcategory &&
                d.group == group) keys.append(d.key);
        }
        return keys;
    }

    QStringList groupsIn(const QString& category) const {
        QStringList groups;
        QSet<QString> seen;
        for (const auto& d : schema_) {
            if (d.category != category) continue;
            if (!seen.contains(d.group)) {
                seen.insert(d.group);
                groups.append(d.group);
            }
        }
        return groups;
    }

    QStringList groupsIn(const QString& category,
                         const QString& subcategory) const {
        QStringList groups;
        QSet<QString> seen;
        for (const auto& d : schema_) {
            if (d.category != category || d.subcategory != subcategory)
                continue;
            if (!seen.contains(d.group)) {
                seen.insert(d.group);
                groups.append(d.group);
            }
        }
        return groups;
    }

    QStringList subcategoriesIn(const QString& category) const {
        QStringList subs;
        QSet<QString> seen;
        for (const auto& d : schema_) {
            if (d.category != category) continue;
            if (!seen.contains(d.subcategory)) {
                seen.insert(d.subcategory);
                subs.append(d.subcategory);
            }
        }
        return subs;
    }

private slots:
    void initTestCase() {
        PPSSPPAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    // ──────── Top-level ────────

    void testTopLevelCategories() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories,
                 QSet<QString>({"Recommended", "Graphics", "Audio",
                                "Networking", "System"}));
    }

    // ──────── Recommended ────────

    void testRecommendedGroupOrder() {
        QCOMPARE(groupsIn("Recommended"),
                 QStringList({
                     "Performance", "Visual Quality", "Frame Pacing",
                     "Audio", "Convenience",
                 }));
    }

    void testRecommendedPerformanceFullCatalog() {
        QCOMPARE(keysIn("Recommended", "Performance"),
                 QStringList({
                     "GraphicsBackend", "SkipBufferEffects",
                     "AutoFrameSkip", "InflightFrames",
                 }));
    }

    void testRecommendedVisualQualityFullCatalog() {
        // No synthetic AspectRatio combo — PPSSPP's standalone UI doesn't
        // expose a discrete aspect-ratio enum, so the AspectRatioPreview
        // beside this group is purely decorative.
        QCOMPARE(keysIn("Recommended", "Visual Quality"),
                 QStringList({
                     "InternalResolution", "MultiSampleLevel",
                     "TextureFiltering", "AnisotropyLevel",
                 }));
    }

    void testRecommendedFramePacingFullCatalog() {
        QCOMPARE(keysIn("Recommended", "Frame Pacing"),
                 QStringList({"VerticalSync", "FrameSkip", "FrameRate"}));
    }

    void testRecommendedAudioFullCatalog() {
        QCOMPARE(keysIn("Recommended", "Audio"),
                 QStringList({"Enable", "AudioSyncMode", "GameVolume"}));
    }

    void testRecommendedConvenienceFullCatalog() {
        QCOMPARE(keysIn("Recommended", "Convenience"),
                 QStringList({"AutoLoadSaveState", "EnableCheats"}));
    }

    void testRecommendedPreviewSpec() {
        // Decorative-only preview: aspect type, empty keyToProperty (no
        // schema key binds — the AspectRatioPreview renders its default
        // 4:3 frame and stays static). GenericSettingsPage's
        // primaryPreviewGroup fallback uses the first group ("Performance")
        // so cards still lay out beside the preview.
        PPSSPPAdapter adapter;
        const PreviewSpec spec = adapter.previewSpec("Recommended", "");
        QCOMPARE(spec.previewType, QString("aspect"));
        QVERIFY(spec.keyToProperty.isEmpty());
    }

    void testGraphicsOSDPreviewSpec() {
        // Same decorative pattern: OsdPreview sits beside the bitmask
        // toggles but isn't wired to them (the toggles share one INI key,
        // which the preview-binding wiring can't disambiguate).
        PPSSPPAdapter adapter;
        const PreviewSpec spec =
            adapter.previewSpec("Graphics", "On-Screen Display");
        QCOMPARE(spec.previewType, QString("osd"));
        QVERIFY(spec.keyToProperty.isEmpty());
    }

    void testSubcategoriesOnlyOnGraphics() {
        // PPSSPP upstream renders every tab as a single scrolling ItemHeader
        // page; we keep that shape for every category EXCEPT Graphics, which
        // we split into General + On-Screen Display sub-tabs to match the
        // pattern Dolphin / PCSX2 / DuckStation use.
        for (const auto& d : schema_) {
            if (d.category == "Graphics") {
                QVERIFY2(d.subcategory == "General" ||
                             d.subcategory == "On-Screen Display",
                         qPrintable(QString("unexpected Graphics subcategory "
                                            "'%1' on key %2")
                                        .arg(d.subcategory, d.key)));
            } else {
                QVERIFY2(d.subcategory.isEmpty(),
                         qPrintable(QString("non-empty subcategory '%1' on "
                                            "key %2 in category %3")
                                        .arg(d.subcategory, d.key, d.category)));
            }
        }
    }

    // ──────── Graphics ────────

    void testGraphicsSubcategories() {
        // Two sub-tabs: General (the upstream GameSettingsScreen content
        // verbatim) + On-Screen Display (the bitmask Show* toggles, with an
        // OsdPreview beside them — matches Dolphin / DuckStation / PCSX2).
        QCOMPARE(subcategoriesIn("Graphics"),
                 QStringList({"General", "On-Screen Display"}));
    }

    void testGraphicsGeneralGroupOrder() {
        // Mirrors CreateGraphicsSettings ItemHeader call order in
        // UI/GameSettingsScreen.cpp:295. Overlay Information is no longer
        // here — it moved to its own On-Screen Display sub-tab.
        QCOMPARE(groupsIn("Graphics", "General"),
                 QStringList({
                     "Rendering Mode",
                     "Display",
                     "Frame Rate Control",
                     "Speed Hacks",
                     "Performance",
                     "Texture upscaling",
                     "Texture Filtering",
                 }));
    }

    void testGraphicsRenderingModeFullCatalog() {
        QCOMPARE(keysIn("Graphics", "General", "Rendering Mode"),
                 QStringList({
                     "GraphicsBackend", "InternalResolution",
                     "SoftwareRenderer", "MultiSampleLevel", "ReplaceTextures",
                 }));
    }

    void testGraphicsDisplayFullCatalog() {
        QCOMPARE(keysIn("Graphics", "General", "Display"),
                 QStringList({"VerticalSync", "LowLatencyPresent"}));
    }

    void testGraphicsFrameRateControlFullCatalog() {
        QCOMPARE(keysIn("Graphics", "General", "Frame Rate Control"),
                 QStringList({
                     "FrameSkip", "AutoFrameSkip", "FrameRate", "FrameRate2",
                 }));
    }

    void testGraphicsSpeedHacksFullCatalog() {
        QCOMPARE(keysIn("Graphics", "General", "Speed Hacks"),
                 QStringList({
                     "SkipBufferEffects", "DisableRangeCulling",
                     "SkipGPUReadbackMode", "DepthRasterMode",
                     "TextureBackoffCache", "SplineBezierQuality", "BloomHack",
                 }));
    }

    void testGraphicsPerformanceFullCatalog() {
        QCOMPARE(keysIn("Graphics", "General", "Performance"),
                 QStringList({
                     "RenderDuplicateFrames", "InflightFrames",
                     "HardwareTransform", "SoftwareSkinning",
                     "HardwareTessellation",
                 }));
    }

    void testGraphicsTextureUpscalingFullCatalog() {
        // GPU texture upscaler (sTextureShaderName) deferred — submodal.
        QCOMPARE(keysIn("Graphics", "General", "Texture upscaling"),
                 QStringList({
                     "TexScalingType", "TexScalingLevel", "TexDeposterize",
                 }));
    }

    void testGraphicsTextureFilteringFullCatalog() {
        QCOMPARE(keysIn("Graphics", "General", "Texture Filtering"),
                 QStringList({
                     "AnisotropyLevel", "TextureFiltering",
                     "Smart2DTexFiltering",
                 }));
    }

    void testGraphicsOSDFullCatalog() {
        // Three bitmask checkboxes share the same iShowStatusFlags key.
        QCOMPARE(keysIn("Graphics", "On-Screen Display", "Overlay Information"),
                 QStringList({
                     "iShowStatusFlags",  // Show FPS Counter
                     "iShowStatusFlags",  // Show Speed
                     "iShowStatusFlags",  // Show Battery %
                 }));
    }

    void testOverlayBitmaskCheckboxes() {
        struct Expected { QString label; int bit; };
        const QVector<Expected> expected = {
            {"Show FPS Counter", 2},
            {"Show Speed",       4},
            {"Show Battery %",   8},
        };
        for (const auto& exp : expected) {
            const SettingDef* found = nullptr;
            for (const auto& d : schema_) {
                if (d.label == exp.label) { found = &d; break; }
            }
            QVERIFY2(found != nullptr, qPrintable("missing: " + exp.label));
            QCOMPARE(found->category, QString("Graphics"));
            QCOMPARE(found->subcategory, QString("On-Screen Display"));
            QCOMPARE(found->group, QString("Overlay Information"));
            QCOMPARE(found->key, QString("iShowStatusFlags"));
            QCOMPARE(int(found->type), int(SettingDef::Bool));
            QCOMPARE(found->bitmask, exp.bit);
        }
    }

    void testRenderingResolutionGate() {
        // Look up the Graphics-tab copy specifically — the Recommended-tab
        // copy doesn't carry the dependsOn gate (gates only resolve within
        // a category's slice, and the Recommended slice doesn't include
        // SoftwareRenderer / SkipBufferEffects).
        for (const auto& d : schema_) {
            if (d.category == "Graphics" && d.key == "InternalResolution") {
                QCOMPARE(d.dependsOn,
                         QString("!SoftwareRenderer && !SkipBufferEffects"));
                return;
            }
        }
        QFAIL("Graphics InternalResolution not found in schema");
    }

    void testLensFlareOcclusionExists() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.key == "DepthRasterMode") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Graphics"));
        QCOMPARE(found->group, QString("Speed Hacks"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->options.size(), 4);  // Auto / Low / Off / Always on
    }

    // ──────── Audio ────────

    void testAudioGroupOrder() {
        QCOMPARE(groupsIn("Audio"),
                 QStringList({
                     "Audio playback", "Game volume", "UI sound", "Audio backend",
                 }));
    }

    void testAudioPlaybackFullCatalog() {
        QCOMPARE(keysIn("Audio", "Audio playback"),
                 QStringList({"AudioSyncMode", "FillAudioGaps"}));
    }

    void testAudioGameVolumeFullCatalog() {
        QCOMPARE(keysIn("Audio", "Game volume"),
                 QStringList({
                     "Enable", "GameVolume", "ReverbRelativeVolume",
                     "AltSpeedRelativeVolume", "AchievementVolume",
                 }));
    }

    void testAudioUISoundFullCatalog() {
        QCOMPARE(keysIn("Audio", "UI sound"),
                 QStringList({"UISound", "UIVolume", "GamePreviewVolume"}));
    }

    void testAudioBackendFullCatalog() {
        QCOMPARE(keysIn("Audio", "Audio backend"),
                 QStringList({"AudioBufferSize", "AutoAudioDevice"}));
    }

    void testFillAudioGapsGate() {
        for (const auto& d : schema_) {
            if (d.key == "FillAudioGaps") {
                QCOMPARE(d.dependsOn, QString("AudioSyncMode=0"));
                return;
            }
        }
        QFAIL("FillAudioGaps not found in schema");
    }

    // ──────── Networking ────────

    void testNetworkingGroupOrder() {
        QCOMPARE(groupsIn("Networking"),
                 QStringList({
                     "Networking",
                     "Ad Hoc multiplayer",
                     "Infrastructure",
                     "UPnP (port-forwarding)",
                     "Chat",
                     "Quick chat",
                     "Misc (default = compatibility)",
                 }));
    }

    void testNetworkingChatGate() {
        // Both chat-position combos and the quick-chat enable toggle gate on
        // EnableNetworkChat upstream.
        QStringList gated;
        for (const auto& d : schema_)
            if (d.dependsOn == "EnableNetworkChat") gated.append(d.key);
        QCOMPARE(gated,
                 QStringList({
                     "ChatButtonPosition", "ChatScreenPosition",
                     "EnableQuickChat",
                 }));
    }

    // ──────── System ────────

    void testSystemGroupOrder() {
        QCOMPARE(groupsIn("System"),
                 QStringList({
                     "UI",
                     "PSP Memory Stick",
                     "Emulation",
                     "Save states",
                     "General",
                     "Cheats",
                     "PSP Settings",
                 }));
    }

    void testSystemEmulationFullCatalog() {
        QCOMPARE(keysIn("System", "Emulation"),
                 QStringList({
                     "FastMemoryAccess", "IgnoreBadMemAccess",
                     "IOTimingMethod", "ForceLagSync2", "CPUSpeed",
                 }));
    }

    void testSystemSaveStatesFullCatalog() {
        QCOMPARE(keysIn("System", "Save states"),
                 QStringList({
                     "EnableStateUndo", "SaveStateSlotCount",
                     "AutoLoadSaveState", "RewindSnapshotInterval",
                 }));
    }

    void testSystemPSPSettingsFullCatalog() {
        // Nickname (sNickName) deferred — text input.
        QCOMPARE(keysIn("System", "PSP Settings"),
                 QStringList({
                     "GameLanguage", "PSPModel", "DayLightSavings",
                     "ParamDateFormat", "ParamTimeFormat", "ButtonPreference",
                 }));
    }

    void testFastMemoryInCpuSection() {
        // Upstream Core/Config.cpp::cpuSettings[] holds FastMemoryAccess +
        // IOTimingMethod + CPUSpeed; section name is "CPU".
        for (const auto& d : schema_) {
            if (d.key == "FastMemoryAccess") {
                QCOMPARE(d.section, QString("CPU"));
                QCOMPARE(d.category, QString("System"));
                return;
            }
        }
        QFAIL("FastMemoryAccess not found in schema");
    }
};

QTEST_GUILESS_MAIN(TestPPSSPPSchema)
#include "test_ppsspp_schema.moc"
