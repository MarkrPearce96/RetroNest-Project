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
        // Synthetic AspectRatio combo is the FIRST entry — it must be the
        // primary preview-bound key for the AspectRatioPreview layout.
        QCOMPARE(keysIn("Recommended", "Visual Quality"),
                 QStringList({
                     "DisplayStretch", "InternalResolution",
                     "MultiSampleLevel", "TextureFiltering",
                     "AnisotropyLevel",
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

    void testRecommendedAspectRatioSynthetic() {
        // The synthetic AspectRatio combo writes to bDisplayStretch in
        // [DisplayLayout.Landscape]. Verify the section/key are wired right
        // and that load/save transforms are present.
        const SettingDef* found = nullptr;
        for (const auto& d : schema_) {
            if (d.category == "Recommended" && d.key == "DisplayStretch") {
                found = &d;
                break;
            }
        }
        QVERIFY(found != nullptr);
        QCOMPARE(found->section, QString("DisplayLayout.Landscape"));
        QCOMPARE(found->label, QString("Aspect Ratio"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->options.size(), 4);  // Auto / 16:9 / 4:3 / Stretch
        QVERIFY(found->saveTransform != nullptr);
        QVERIFY(found->loadTransform != nullptr);
    }

    void testRecommendedPreviewSpec() {
        PPSSPPAdapter adapter;
        const PreviewSpec spec = adapter.previewSpec("Recommended", "");
        QCOMPARE(spec.previewType, QString("aspect"));
        QVERIFY(spec.keyToProperty.contains("DisplayStretch"));
        QCOMPARE(spec.keyToProperty.value("DisplayStretch"),
                 QString("aspectMode"));
    }

    void testNoSubcategoriesAnywhere() {
        // PPSSPP upstream renders every tab as a single scrolling page with
        // ItemHeader sections — no sub-tabs. Subcategory must be empty for
        // every entry so GenericSettingsPage falls back to single-page mode.
        for (const auto& d : schema_) {
            QVERIFY2(d.subcategory.isEmpty(),
                     qPrintable(QString("non-empty subcategory '%1' on key %2")
                                    .arg(d.subcategory, d.key)));
        }
    }

    // ──────── Graphics ────────

    void testGraphicsGroupOrder() {
        // Mirrors CreateGraphicsSettings ItemHeader call order in
        // UI/GameSettingsScreen.cpp:295.
        QCOMPARE(groupsIn("Graphics"),
                 QStringList({
                     "Rendering Mode",
                     "Display",
                     "Frame Rate Control",
                     "Speed Hacks",
                     "Performance",
                     "Texture upscaling",
                     "Texture Filtering",
                     "Overlay Information",
                 }));
    }

    void testGraphicsRenderingModeFullCatalog() {
        QCOMPARE(keysIn("Graphics", "Rendering Mode"),
                 QStringList({
                     "GraphicsBackend", "InternalResolution",
                     "SoftwareRenderer", "MultiSampleLevel", "ReplaceTextures",
                 }));
    }

    void testGraphicsDisplayFullCatalog() {
        QCOMPARE(keysIn("Graphics", "Display"),
                 QStringList({"VerticalSync", "LowLatencyPresent"}));
    }

    void testGraphicsFrameRateControlFullCatalog() {
        QCOMPARE(keysIn("Graphics", "Frame Rate Control"),
                 QStringList({
                     "FrameSkip", "AutoFrameSkip", "FrameRate", "FrameRate2",
                 }));
    }

    void testGraphicsSpeedHacksFullCatalog() {
        QCOMPARE(keysIn("Graphics", "Speed Hacks"),
                 QStringList({
                     "SkipBufferEffects", "DisableRangeCulling",
                     "SkipGPUReadbackMode", "DepthRasterMode",
                     "TextureBackoffCache", "SplineBezierQuality", "BloomHack",
                 }));
    }

    void testGraphicsPerformanceFullCatalog() {
        QCOMPARE(keysIn("Graphics", "Performance"),
                 QStringList({
                     "RenderDuplicateFrames", "InflightFrames",
                     "HardwareTransform", "SoftwareSkinning",
                     "HardwareTessellation",
                 }));
    }

    void testGraphicsTextureUpscalingFullCatalog() {
        // GPU texture upscaler (sTextureShaderName) deferred — submodal.
        QCOMPARE(keysIn("Graphics", "Texture upscaling"),
                 QStringList({
                     "TexScalingType", "TexScalingLevel", "TexDeposterize",
                 }));
    }

    void testGraphicsTextureFilteringFullCatalog() {
        QCOMPARE(keysIn("Graphics", "Texture Filtering"),
                 QStringList({
                     "AnisotropyLevel", "TextureFiltering",
                     "Smart2DTexFiltering",
                 }));
    }

    void testGraphicsOverlayInformationFullCatalog() {
        // Three bitmask checkboxes share the same iShowStatusFlags key.
        QCOMPARE(keysIn("Graphics", "Overlay Information"),
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
