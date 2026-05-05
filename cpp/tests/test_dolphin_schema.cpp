#include <QtTest>
#include <QSet>
#include "adapters/dolphin_adapter.h"
#include "core/setting_def.h"

class TestDolphinSchema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

private slots:
    void initTestCase() {
        DolphinAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void testTopLevelCategories() {
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({
            "Recommended", "Audio", "General", "Advanced",
            "Graphics", "GameCube", "Wii"
        }));
    }

    void testInterfaceCategoryNotInSchema() {
        // Dolphin's Interface settings only affect Dolphin's own UI,
        // which RetroNest hides entirely. The two embedding-critical
        // keys (PauseOnFocusLost, ConfirmStop) are force-patched by
        // the adapter — they don't appear in the visible schema.
        for (const auto& d : schema_)
            QVERIFY2(d.category != "Interface",
                qPrintable(QString("Interface key '%1' should not be in schema "
                                   "(force-patched by adapter instead)").arg(d.key)));
    }

    void testAudioCategoryFullCatalog() {
        // Mirrors DolphinQt AudioPane (Source/Core/DolphinQt/Settings/AudioPane.cpp).
        const QSet<QString> expectedKeys{
            // Output
            "Backend", "Volume", "MuteOnDisabledSpeedLimit",
            // DSP Emulation
            "DSPHLE", "EnableJIT",
            // Latency & Quality
            "AudioLatency", "AudioBufferSize", "AudioFillGaps", "AudioPreservePitch",
            // Surround
            "DPL2Decoder", "DPL2Quality",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Audio") got.insert(d.key);
        QCOMPARE(got, expectedKeys);
    }

    void testAudioBackendIsCubebDefault() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "DSP" && d.key == "Backend") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->defaultValue, QString("Cubeb"));
        QVERIFY(found->options.size() >= 2);
    }

    void testCpuCoreCombo() {
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "Core" && d.key == "CPUCore") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->category, QString("Advanced"));
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QVERIFY(found->options.size() >= 3);
    }

    void testGeneralCategoryFullCatalog() {
        // Mirrors DolphinQt GeneralPane (Source/Core/DolphinQt/Settings/
        // GeneralPane.cpp). Power-user knobs from upstream's Advanced
        // pane live in our top-level Advanced category — see
        // testAdvancedCategoryFullCatalog.
        const QSet<QString> expectedKeys{
            // Basic Settings
            "CPUThread", "SkipIPL", "EnableCheats", "OverrideRegionSettings",
            "AutoDiscChange", "EmulationSpeed", "LoadGameIntoMemory",
            // (UseDiscordPresence intentionally omitted: wrapped in
            // #ifdef USE_DISCORD_PRESENCE upstream, not built on macOS.)
            // Fallback Region
            "FallbackRegion",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "General") got.insert(d.key);
        QCOMPARE(got, expectedKeys);
    }

    void testAdvancedCategoryFullCatalog() {
        // Mirrors DolphinQt AdvancedPane (Source/Core/DolphinQt/Settings/
        // AdvancedPane.cpp).
        const QSet<QString> expectedKeys{
            // CPU Options
            "CPUCore", "MMU", "AccurateCPUCache", "PauseOnPanic",
            // Clock Override
            "OverclockEnable", "Overclock",
            // VBI Frequency Override
            "VIOverclockEnable", "VIOverclock",
            // Memory Override
            "RAMOverrideEnable",
            // Timing
            "CorrectTimeDrift", "RushFramePresentation", "SmoothEarlyPresentation",
            // Custom RTC Options
            "EnableCustomRTC",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Advanced") got.insert(d.key);
        QCOMPARE(got, expectedKeys);
    }

    void testBoolValuesAreCapitalized() {
        // Dolphin writes True/False (Common/StringUtil.cpp:289-292), not true/false.
        for (const auto& d : schema_) {
            if (d.type != SettingDef::Bool) continue;
            QVERIFY2(d.defaultValue == "True" || d.defaultValue == "False",
                     qPrintable(QString("Bool '%1' has non-capitalized default '%2'")
                                .arg(d.key, d.defaultValue)));
        }
    }

    void testResolutionOptionsTargetGfxIni() {
        DolphinAdapter adapter;
        ResolutionOptions opts = adapter.resolutionOptions();
        QVERIFY(!opts.options.isEmpty());
        QVERIFY2(!opts.iniFilePath.isEmpty(),
                 "Resolution must target GFX.ini via the iniFilePath field");
        QVERIFY(opts.iniFilePath.endsWith("GFX.ini"));
        QCOMPARE(opts.section, QString("Settings"));
        QCOMPARE(opts.key, QString("InternalResolution"));
    }

    void testAspectRatioOptionsTargetGfxIni() {
        DolphinAdapter adapter;
        AspectRatioOptions opts = adapter.aspectRatioOptions();
        QVERIFY(!opts.options.isEmpty());
        for (const auto& opt : opts.options) {
            QVERIFY(!opt.patches.isEmpty());
            for (const auto& patch : opt.patches) {
                QVERIFY2(!patch.iniFilePath.isEmpty(),
                         qPrintable(QString("Aspect '%1' patch missing iniFilePath").arg(opt.label)));
                QVERIFY(patch.iniFilePath.endsWith("GFX.ini"));
            }
        }
    }

    void testGraphicsSubTabsCovered() {
        // Full Graphics audit: sub-tabs match DolphinQt's GraphicsPane
        // structure exactly.
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics") got.insert(d.subcategory);
        QCOMPARE(got, QSet<QString>({
            "General", "Enhancements", "Hacks", "Advanced", "On-Screen Display"
        }));
    }

    void testGraphicsCategoryHasMinimumExpectedKeys() {
        // Headline keys per sub-tab — guards against accidental drops.
        QSet<QString> graphicsKeys;
        for (const auto& d : schema_)
            if (d.category == "Graphics") graphicsKeys.insert(d.key);

        const QStringList headline{
            "GFXBackend", "AspectRatio", "InternalResolution", "VSync",
            "Fullscreen", "ShaderCompilationMode",
            "MSAA", "SSAA", "MaxAnisotropy", "EnablePixelLighting",
            "ForceTextureFiltering", "StereoMode",
            "EFBToTextureEnable", "XFBToTextureEnable", "BBoxEnable",
            "VertexRounding", "FastDepthCalc",
            "HiresTextures", "DumpTextures", "BitrateKbps",
            "ShowFPS", "ShowSpeed", "OnScreenDisplayMessages",
        };
        for (const QString& k : headline)
            QVERIFY2(graphicsKeys.contains(k), qPrintable("missing key: " + k));
    }

    void testRecommendedHasAspectPreview() {
        DolphinAdapter a;
        // Aspect preview moved from Graphics/General to Recommended —
        // Recommended is the primary entry point and already has the
        // AspectRatio combo. Recommended is single-subcategory →
        // subcategory == "".
        const auto spec = a.previewSpec("Recommended", "");
        QCOMPARE(spec.previewType, QString("aspect"));
        QCOMPARE(spec.keyToProperty.value("AspectRatio"), QString("aspectMode"));
    }

    void testRecommendedCategoryHasMinimumSet() {
        // Curated subset of the most-tweaked Dolphin settings (per the
        // official performance guide + community consensus). Each entry
        // also exists under its primary category — Recommended is a
        // VIEW for fast access. Headline keys must all be present.
        QSet<QString> recommendedKeys;
        for (const auto& d : schema_)
            if (d.category == "Recommended") recommendedKeys.insert(d.key);

        const QStringList headline{
            // Performance
            "GFXBackend", "CPUThread", "ShaderCompilationMode",
            "WaitForShadersBeforeStarting",
            // Performance hacks
            "EFBToTextureEnable", "XFBToTextureEnable", "EFBAccessEnable",
            "SafeTextureCacheColorSamples",
            // Visual quality
            "InternalResolution", "AspectRatio", "wideScreenHack",
            "MSAA", "MaxAnisotropy", "ForceTextureFiltering",
            // Audio
            "DSPHLE", "Volume",
            // Convenience
            "SkipIPL", "EnableCheats",
        };
        for (const QString& k : headline)
            QVERIFY2(recommendedKeys.contains(k),
                     qPrintable("Recommended missing key: " + k));
    }

    void testRecommendedDuplicatesUnderlyingKeys() {
        // The Recommended category duplicates schema entries — both write
        // the same INI section/key as the primary category. Verify a
        // sample: AspectRatio appears under both Recommended and Graphics.
        int aspectCount = 0;
        for (const auto& d : schema_)
            if (d.key == "AspectRatio") aspectCount++;
        QVERIFY2(aspectCount >= 2,
                 "AspectRatio expected under Recommended AND Graphics");
    }

    void testWiiCategoryFullCatalog() {
        // Mirrors DolphinQt WiiPane (Source/Core/DolphinQt/Settings/
        // WiiPane.cpp) — limited to Dolphin.ini-routed keys; SYSCONF_*
        // keys (system menu language, widescreen, PAL60, sensor bar,
        // etc.) are deliberately skipped because they live in the Wii's
        // SYSCONF binary, not a text INI.
        const QSet<QString> expectedKeys{
            "WiiSDCard", "WiiSDCardAllowWrites", "WiiSDCardEnableFolderSync",
            "WiiSDCardFilesize", "WiiKeyboard", "EnableWiiLink",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Wii") got.insert(d.key);
        QCOMPARE(got, expectedKeys);
    }

    void testGameCubeCategoryFullCatalog() {
        // Mirrors DolphinQt GameCubePane (Source/Core/DolphinQt/Settings/
        // GameCubePane.cpp). 4 user-facing keys.
        const QSet<QString> expectedKeys{
            "SelectedLanguage", "SlotA", "SlotB", "SerialPort1",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "GameCube") got.insert(d.key);
        QCOMPARE(got, expectedKeys);
    }

    void testGraphicsSubTabsHaveNoPreviewExceptOsd() {
        // Graphics/General used to host the aspect preview but it moved
        // to Recommended. The On-Screen Display sub-tab gained an OSD
        // preview (driven by ShowFPS/ShowVPS/ShowSpeed/ShowFTimes); the
        // other sub-tabs stay preview-less.
        DolphinAdapter a;
        QVERIFY(a.previewSpec("Graphics", "General").previewType.isEmpty());
        QVERIFY(a.previewSpec("Graphics", "Enhancements").previewType.isEmpty());
        QVERIFY(a.previewSpec("Graphics", "Hacks").previewType.isEmpty());
        QVERIFY(a.previewSpec("Graphics", "Advanced").previewType.isEmpty());
        const auto osd = a.previewSpec("Graphics", "On-Screen Display");
        QCOMPARE(osd.previewType, QString("osd"));
        QVERIFY(osd.keyToProperty.contains("ShowFPS"));
        QVERIFY(osd.keyToProperty.contains("ShowSpeed"));
    }
};

QTEST_MAIN(TestDolphinSchema)
#include "test_dolphin_schema.moc"
