#include <QtTest>
#include <QSet>
#include "adapters/duckstation_adapter.h"
#include "core/setting_def.h"

class TestDuckStationSchema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

    QSet<QString> keysFor(const QString& category,
                          const QString& subcategory = {}) const {
        QSet<QString> out;
        for (const auto& d : schema_) {
            if (d.category != category) continue;
            if (!subcategory.isEmpty() && d.subcategory != subcategory) continue;
            out.insert(d.key);
        }
        return out;
    }

    QSet<QString> subcategoriesFor(const QString& category) const {
        QSet<QString> out;
        for (const auto& d : schema_)
            if (d.category == category) out.insert(d.subcategory);
        return out;
    }

private slots:
    void initTestCase() {
        DuckStationAdapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void testTopLevelCategories() {
        // Mirrors upstream SettingsWindow::addPages (settingswindow.cpp:92).
        // Omitted vs upstream:
        //   - Interface  (Dolphin precedent: emulator-UI-only, RetroNest hides it)
        //   - Game List  (RetroNest manages the game list)
        //   - Post-Processing (filesystem-scanned shader picker — deferral blocker)
        //   - Debugging  (gated on QtHost::ShouldShowDebugOptions, hidden by default)
        //   - Patches/Cheats (per-game only)
        QSet<QString> categories;
        for (const auto& d : schema_) categories.insert(d.category);
        QCOMPARE(categories, QSet<QString>({
            "Recommended",
            "Console", "Emulation", "Memory Cards",
            "Graphics", "On-Screen Display", "Audio",
            "Achievements", "Capture", "Advanced",
        }));
    }

    void testRecommendedCategoryFullCatalog() {
        // Curated cross-cut of the most-tweaked DuckStation settings.
        // These INI keys ALSO appear under their primary category — both
        // entries write the same section/key so editing in either place
        // produces the same result. Mirrors dolphin_adapter / pcsx2_adapter.
        QCOMPARE(keysFor("Recommended"), QSet<QString>({
            // Performance
            "Renderer", "ExecutionMode", "UseThread", "ReadSpeedup",
            // Visual Quality (AspectRatio drives the live aspect preview)
            "ResolutionScale", "AspectRatio", "WidescreenHack",
            "PGXPEnable", "Multisamples", "TextureFilter",
            // Frame Pacing
            "VSync", "SyncToHostRefreshRate", "OptimalFramePacing",
            // Audio
            "Backend", "OutputVolume",
            // Convenience
            "EmulationSpeed", "Region", "PatchFastBoot",
        }));
    }

    void testRecommendedHasAspectPreview() {
        DuckStationAdapter adapter;
        const PreviewSpec spec = adapter.previewSpec("Recommended", "");
        QCOMPARE(spec.previewType, QString("aspect"));
        QVERIFY(spec.keyToProperty.contains("AspectRatio"));
    }

    void testRecommendedDuplicatesUnderlyingKeys() {
        // Every Recommended entry must duplicate (not replace) a key that
        // exists under another category — otherwise the curated view
        // would diverge from the full panes.
        QSet<QPair<QString,QString>> primaryKeys;
        for (const auto& d : schema_)
            if (d.category != "Recommended")
                primaryKeys.insert(qMakePair(d.section, d.key));
        for (const auto& d : schema_) {
            if (d.category != "Recommended") continue;
            QVERIFY2(primaryKeys.contains(qMakePair(d.section, d.key)),
                qPrintable(QString("Recommended key '%1/%2' has no primary "
                                   "category counterpart — Recommended must be "
                                   "a curated VIEW, not a primary owner")
                           .arg(d.section, d.key)));
        }
    }

    void testInterfaceCategoryNotInSchema() {
        for (const auto& d : schema_)
            QVERIFY2(d.category != "Interface",
                qPrintable(QString("Interface key '%1' should not be in schema "
                                   "(force-patched by adapter instead)").arg(d.key)));
    }

    void testBoolDefaultsAreLowercase() {
        // DuckStation IniSettingsInterface writes lowercase true/false.
        for (const auto& d : schema_) {
            if (d.type != SettingDef::Bool) continue;
            QVERIFY2(d.defaultValue == "true" || d.defaultValue == "false",
                     qPrintable(QString("Bool '%1' has non-lowercase default '%2'")
                                .arg(d.key, d.defaultValue)));
        }
    }

    // ── Per-category catalogs — each is the visible upstream pane in
    //    addRow order, exact INI keys, no surplus and no omissions. ───────

    void testConsoleCategoryFullCatalog() {
        QCOMPARE(keysFor("Console"), QSet<QString>({
            // Console group
            "Region", "ForceVideoTiming", "PatchFastBoot", "FastForwardBoot",
            "FastForwardAccess", "Enable8MBRAM",
            // CPU Emulation group
            "ExecutionMode", "OverclockEnable", "OverclockNumerator", "RecompilerICache",
            // CD-ROM Emulation group
            "ReadSpeedup", "SeekSpeedup", "LoadImageToRAM", "LoadImagePatches",
            "AutoDiscChange", "IgnoreHostSubcode",
        }));
    }

    void testEmulationCategoryFullCatalog() {
        QCOMPARE(keysFor("Emulation"), QSet<QString>({
            // Speed Control
            "EmulationSpeed", "FastForwardSpeed", "TurboSpeed",
            // Latency Control
            "VSync", "SyncToHostRefreshRate", "OptimalFramePacing",
            "PreFrameSleep", "SkipPresentingDuplicateFrames", "PreFrameSleepBuffer",
            // Rewind
            "RewindEnable", "RewindFrequency", "RewindSaveSlots",
            "UseSoftwareRendererForMemoryStates",
            // Runahead
            "RunaheadFrameCount", "RunaheadForAnalogInput",
        }));
    }

    void testMemoryCardsCategoryFullCatalog() {
        // Save Locations group (Folders/Memcards + Folders/SaveStates) is
        // omitted — RetroNest manages those paths under
        // emulators/duckstation/{systemId}/{memcards,savestates}.
        QCOMPARE(keysFor("Memory Cards"), QSet<QString>({
            "Card1Type", "Card1Path", "Card2Type", "Card2Path", "UsePlaylistTitle",
        }));
    }

    void testGraphicsSubTabsCovered() {
        // Upstream graphicssettingswidget.ui has 5 tabs (basicTab "Rendering",
        // advancedTab "Advanced", pgxpTab "PGXP", tabTextureReplacements
        // "Texture Replacement", debugTab "Debugging"). The Debugging tab is
        // gated on QtHost::ShouldShowDebugOptions — dropped from our schema
        // matching the PCSX2 precedent.
        QCOMPARE(subcategoriesFor("Graphics"), QSet<QString>({
            "Rendering", "Advanced", "PGXP", "Texture Replacement",
        }));
    }

    void testGraphicsRenderingFullCatalog() {
        // Adapter (GPU/Adapter) deferred — populated dynamically per renderer.
        // DownsampleScale deferred — upstream visibility-gates on
        // DownsampleMode==Box; our DSL only greys out, doesn't hide.
        QCOMPARE(keysFor("Graphics", "Rendering"), QSet<QString>({
            "Renderer",
            "ResolutionScale", "DownsampleMode",
            "TextureFilter", "SpriteTextureFilter",
            "DitheringMode", "DeinterlacingMode",
            "AspectRatio", "CropMode", "Scaling", "Scaling24Bit",
            "PGXPEnable", "PGXPDepthBuffer",
            "Force4_3For24Bit", "ChromaSmoothing24Bit",
            "WidescreenHack", "ForceRoundTextureCoordinates",
        }));
    }

    void testGraphicsAdvancedFullCatalog() {
        // FullscreenMode + ExclusiveFullscreenControl deferred — runtime-
        // populated from adapter.fullscreen_modes / Vulkan-only.
        // UseBlitSwapChain dropped — Windows + D3D11 only upstream.
        QCOMPARE(keysFor("Graphics", "Advanced"), QSet<QString>({
            // Display Options
            "Alignment", "Rotation", "FineCropMode",
            "FineCropLeft", "FineCropTop", "FineCropRight", "FineCropBottom",
            "DisableMailboxPresentation",
            // Rendering Options
            "Multisamples", "LineDetectMode", "UseThread", "MaxQueuedFrames",
            "EnableModulationCrop", "ScaledInterlacing", "UseSoftwareRendererForReadbacks",
        }));
    }

    void testGraphicsPgxpFullCatalog() {
        // PGXPTolerance + PGXPDepthThreshold deferred — float spinbox blocker.
        QCOMPARE(keysFor("Graphics", "PGXP"), QSet<QString>({
            "PGXPTextureCorrection", "PGXPColorCorrection",
            "PGXPCulling", "PGXPPreserveProjFP", "PGXPCPU",
            "PGXPVertexCache", "PGXPDisableOn2DPolygons",
            "PGXPTransparentDepthTest",
        }));
    }

    void testGraphicsTextureReplacementFullCatalog() {
        // Folders/Textures (textures dir) deferred — RetroNest-managed.
        QCOMPARE(keysFor("Graphics", "Texture Replacement"), QSet<QString>({
            "EnableTextureCache", "PreloadTextures",
            "EnableTextureReplacements", "DumpTextures",
            "AlwaysTrackUploads", "DumpReplacedTextures",
            "EnableVRAMWriteReplacements", "DumpVRAMWrites",
            "UseOldMDECRoutines",
        }));
    }

    void testOnScreenDisplayFullCatalog() {
        // Theme / Font / Overlay Font deferred — runtime-populated combos.
        QCOMPARE(keysFor("On-Screen Display"), QSet<QString>({
            // Display
            "OSDScale", "OSDMargin",
            // Messages
            "ShowOSDMessages", "ShowStatusIndicators", "AnimateOSDMessages",
            "BlurOSDMessageBackgrounds",
            "OSDErrorDuration", "OSDWarningDuration", "OSDInfoDuration",
            "OSDQuickDuration", "OSDMessageLocation",
            // Overlays
            "ShowFPS", "ShowSpeed", "ShowCPU", "ShowGPU",
            "ShowResolution", "ShowGPUStatistics", "ShowFrameTimes",
            "ShowLatencyStatistics", "ShowInputs", "ShowEnhancements",
        }));
    }

    void testAudioFullCatalog() {
        // Driver + OutputDevice are present but their option lists are
        // placeholder stubs (deferred — async device enumeration blocker).
        QCOMPARE(keysFor("Audio"), QSet<QString>({
            // Controls
            "OutputVolume", "FastForwardVolume", "OutputMuted", "MuteCDAudio",
            // Configuration
            "Backend", "Driver", "OutputDevice", "StretchMode",
            "BufferMS", "OutputLatencyMS", "OutputLatencyMinimal",
            // Time Stretching
            "StretchSequenceLengthMS", "StretchSeekWindowMS", "StretchOverlapMS",
            "StretchUseQuickSeek", "StretchUseAAFilter",
        }));
    }

    void testAchievementsFullCatalog() {
        // Login box (credentials) handled by RetroNest's RAService.
        // NotificationScale + IndicatorScale (auto/osd-scale/custom + spinbox
        // composite) deferred.
        QCOMPARE(keysFor("Achievements"), QSet<QString>({
            // Settings
            "Enabled", "ChallengeMode", "SpectatorMode", "EncoreMode",
            "UnofficialTestMode", "PrefetchBadges",
            // Notifications
            "Notifications", "NotificationsDuration",
            "LeaderboardNotifications", "LeaderboardsDuration",
            "LeaderboardTrackers", "SoundEffects", "NotificationLocation",
            // Progress Tracking
            "ChallengeIndicatorMode", "ProgressIndicatorMode", "IndicatorLocation",
        }));
    }

    void testCaptureFullCatalog() {
        // Screenshot/video Save Locations deferred — RetroNest-managed.
        QCOMPARE(keysFor("Capture"), QSet<QString>({
            // Screenshots
            "ScreenshotMode", "ScreenshotFormat", "ScreenshotQuality",
            "ScreenshotFileNameFormat",
            // Media Capture
            "Backend", "Container", "FilenameFormat",
            "VideoCapture", "VideoCodec", "VideoBitrate",
            "VideoAutoSize", "VideoWidth", "VideoHeight",
            "VideoCodecUseArgs", "VideoCodecArgs",
            "AudioCapture", "AudioCodec", "AudioBitrate",
            "AudioCodecUseArgs", "AudioCodecArgs",
        }));
    }

    void testAdvancedCategoryFullCatalog() {
        // Cache + Covers folder pickers omitted (RetroNest-managed).
        // ShowDebugMenu omitted — toggles upstream Debugging panes that we
        // don't expose. Log Channels (popup menu) deferred.
        QCOMPARE(keysFor("Advanced"), QSet<QString>({
            "LogLevel", "LogToConsole", "LogToDebug", "LogToWindow", "LogToFile",
            "LogTimestamps", "LogFileTimestamps",
        }));
    }

    // ── Specific shape checks ────────────────────────────────────────────

    void testEmulationSpeedComboMatchesUpstream() {
        // Upstream emulationsettingswidget.cpp:171-189 builds
        // "Unlimited" + 25 percentages. INI values are shortest-form floats.
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "Main" && d.key == "EmulationSpeed") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(int(found->type), int(SettingDef::Combo));
        QCOMPARE(found->options.size(), 26);
        QCOMPARE(found->options.first().first,  QString("Unlimited"));
        QCOMPARE(found->options.first().second, QString("0"));
        // 100% → "1" (shortest-form, audit 2026-04-06 compliance).
        bool found100 = false;
        for (const auto& opt : found->options)
            if (opt.first.startsWith("100%")) { QCOMPARE(opt.second, QString("1")); found100 = true; break; }
        QVERIFY(found100);
    }

    void testCpuOverclockSliderHasFractionTransforms() {
        // Mirrors Settings::CPUOverclockPercentToFraction (settings.cpp:230):
        // slider stores percent; on save we write Numerator + Denominator
        // as a gcd-reduced fraction.
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "CPU" && d.key == "OverclockNumerator") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(int(found->type), int(SettingDef::Int));
        QVERIFY(found->saveTransform != nullptr);
        QVERIFY(found->loadTransform != nullptr);

        QHash<QPair<QString,QString>, QString> writes;
        auto saveCb = [&writes](const QString& sec, const QString& k, const QString& v) {
            writes[qMakePair(sec, k)] = v;
        };

        // 100% → 1/1
        writes.clear();
        found->saveTransform("100", saveCb);
        QCOMPARE(writes.value(qMakePair(QString("CPU"), QString("OverclockNumerator"))),
                 QString("1"));
        QCOMPARE(writes.value(qMakePair(QString("CPU"), QString("OverclockDenominator"))),
                 QString("1"));

        // 25% → 1/4
        writes.clear();
        found->saveTransform("25", saveCb);
        QCOMPARE(writes.value(qMakePair(QString("CPU"), QString("OverclockNumerator"))),
                 QString("1"));
        QCOMPARE(writes.value(qMakePair(QString("CPU"), QString("OverclockDenominator"))),
                 QString("4"));

        // 150% → 3/2
        writes.clear();
        found->saveTransform("150", saveCb);
        QCOMPARE(writes.value(qMakePair(QString("CPU"), QString("OverclockNumerator"))),
                 QString("3"));
        QCOMPARE(writes.value(qMakePair(QString("CPU"), QString("OverclockDenominator"))),
                 QString("2"));

        // loadTransform recovers percent from numerator/denominator.
        auto readCb = [](const QString& sec, const QString& k) -> QString {
            if (sec == "CPU" && k == "OverclockNumerator")   return "3";
            if (sec == "CPU" && k == "OverclockDenominator") return "2";
            return "";
        };
        QCOMPARE(found->loadTransform(readCb), QString("150"));
    }

    void testStretchModeOptionsMatchUpstream() {
        // Display labels from s_stretch_mode_display_names
        // (core_audio_stream.cpp:230-234), INI values from
        // s_stretch_mode_names (core_audio_stream.cpp:225-229).
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "Audio" && d.key == "StretchMode") found = &d;
        QVERIFY(found != nullptr);
        QCOMPARE(found->options.size(), 3);
        // Default = TimeStretch.
        QCOMPARE(found->defaultValue, QString("TimeStretch"));
        QStringList values;
        for (const auto& opt : found->options) values.append(opt.second);
        QCOMPARE(QSet<QString>(values.begin(), values.end()), QSet<QString>({"None", "Resample", "TimeStretch"}));
    }

    void testAudioBackendIncludesNullCubebSdl() {
        // s_backend_names (audio_stream.cpp:20-34) on macOS = Null/Cubeb/SDL.
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "Audio" && d.key == "Backend") found = &d;
        QVERIFY(found != nullptr);
        QStringList values;
        for (const auto& opt : found->options) values.append(opt.second);
        QCOMPARE(QSet<QString>(values.begin(), values.end()), QSet<QString>({"Null", "Cubeb", "SDL"}));
        QCOMPARE(found->defaultValue, QString("Cubeb"));
    }

    void testAchievementChallengeIndicatorOptionsMatchUpstream() {
        // s_achievement_challenge_indicator_mode_names (settings.cpp:2460-2465).
        const SettingDef* found = nullptr;
        for (const auto& d : schema_)
            if (d.section == "Cheevos" && d.key == "ChallengeIndicatorMode") found = &d;
        QVERIFY(found != nullptr);
        QStringList values;
        for (const auto& opt : found->options) values.append(opt.second);
        QCOMPARE(QSet<QString>(values.begin(), values.end()), QSet<QString>({
            "Disabled", "PersistentIcon", "TemporaryIcon", "Notification"
        }));
    }

    void testRenderingHasNoPreviewSpec() {
        // Aspect preview moved to Recommended — see testRecommendedHasAspectPreview.
        DuckStationAdapter adapter;
        const PreviewSpec spec = adapter.previewSpec("Graphics", "Rendering");
        QVERIFY(spec.previewType.isEmpty());
    }

    void testOsdHasOsdPreviewSpec() {
        DuckStationAdapter adapter;
        const PreviewSpec spec = adapter.previewSpec("On-Screen Display", "");
        QCOMPARE(spec.previewType, QString("osd"));
        QVERIFY(spec.keyToProperty.contains("ShowFPS"));
    }
};

QTEST_MAIN(TestDuckStationSchema)
#include "test_duckstation_schema.moc"
