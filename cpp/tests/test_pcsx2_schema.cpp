#include <QtTest>
#include <QSet>
#include "adapters/pcsx2_adapter.h"
#include "core/setting_def.h"

// Full-catalog tests for the schema-driven PCSX2 dialog. The bug-net is the
// per-category exact-set assertions: any addition or removal trips the test
// so accidental drift from upstream PCSX2's pane structure is caught loud.

class TestPcsx2Schema : public QObject {
    Q_OBJECT

private:
    QVector<SettingDef> schema_;

private slots:
    void initTestCase() {
        PCSX2Adapter adapter;
        schema_ = adapter.settingsSchema();
        QVERIFY(!schema_.isEmpty());
    }

    void testTopLevelCategories() {
        // Mirrors PCSX2's SettingsWindow.cpp pane list, minus categories that
        // RetroNest manages or that are per-game-only upstream:
        //   skip Interface, Game List, Patches, Cheats, Game Fixes, Folders.
        QSet<QString> got;
        for (const auto& d : schema_) got.insert(d.category);
        QCOMPARE(got, QSet<QString>({
            "Emulation", "Graphics", "Audio", "Memory Cards",
            "Network & HDD", "Achievements", "Advanced",
        }));
    }

    void testInterfaceCategoryNotInSchema() {
        // Interface controls PCSX2's own UI which RetroNest hides; the
        // embedding-critical UI keys are force-patched in patchExistingConfig.
        for (const auto& d : schema_)
            QVERIFY2(d.category != "Interface",
                qPrintable(QString("Interface key '%1' should not be in schema "
                                   "(force-patched by adapter instead)").arg(d.key)));
    }

    void testBiosCategoryNotInSchema() {
        // BIOS pane is collapsed into Emulation > System Settings — there's
        // no separate top-level BIOS category. The BIOS-file picker itself
        // is RetroNest-managed via the wizard.
        for (const auto& d : schema_)
            QVERIFY2(d.category != "BIOS",
                qPrintable(QString("BIOS key '%1' should not be in schema")
                           .arg(d.key)));
    }

    void testEmulationCategoryFullCatalog() {
        // Mirrors EmulationSettingsWidget plus BIOS pane's two Fast Boot
        // toggles (folded in here under System Settings — see settingsSchema's
        // Emulation comment). Per-game-only fastCDVD + Real-Time Clock group
        // are omitted.
        const QSet<QString> expected{
            // Speed Control
            "NominalScalar", "TurboScalar", "SlomoScalar",
            // System Settings
            "EECycleRate", "EECycleSkip", "vuThread", "EnableThreadPinning",
            "EnableCheats", "HostFs", "CdvdPrecache",
            "EnableFastBoot", "EnableFastBootFastForward",
            // Frame Pacing
            "VsyncQueueSize", "SyncToHostRefreshRate", "VsyncEnable",
            "UseVSyncForTiming", "SkipDuplicateFrames",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Emulation") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsSubTabsCovered() {
        // Mirrors GraphicsSettingsWidget — every visible sub-tab.
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics") got.insert(d.subcategory);
        QCOMPARE(got, QSet<QString>({
            "Display", "Rendering", "Hardware Fixes", "Upscaling Fixes",
            "Texture Replacement", "Post-Processing", "Media Capture",
            "Advanced", "On-Screen Display",
        }));
    }

    void testGraphicsDisplayFullCatalog() {
        // Mirrors GraphicsDisplaySettingsTab.ui. Adapter + Fullscreen Mode
        // combos are deferred (dynamically populated at runtime).
        const QSet<QString> expected{
            "Renderer", "AspectRatio", "FMVAspectRatioSwitch",
            "deinterlace_mode", "linear_present_mode",
            "StretchY", "CropLeft", "CropTop", "CropRight", "CropBottom",
            "EnableWideScreenPatches", "EnableNoInterlacingPatches",
            "pcrtc_antiblur", "IntegerScaling", "pcrtc_offsets",
            "disable_interlace_offset", "pcrtc_overscan",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Display") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsRenderingFullCatalog() {
        // Mirrors GraphicsHardwareRenderingSettingsTab.ui +
        // GraphicsSoftwareRenderingSettingsTab.ui collapsed into one sub-tab
        // gated by the Renderer combo. Manual Hardware Renderer Fixes is
        // per-game/dev-build only — omitted.
        const QSet<QString> expected{
            // Shared
            "upscale_multiplier", "filter", "TriFilter", "MaxAnisotropy",
            "dithering_ps2", "accurate_blending_unit",
            // Hardware Rendering Options
            "hw_mipmap", "HWAccurateAlphaTest", "HWAA1",
            // Software Renderer
            "extrathreads", "autoflush_sw", "mipmap",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Rendering") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsHardwareFixesFullCatalog() {
        const QSet<QString> expected{
            "UserHacks_CPUSpriteRenderBW", "UserHacks_CPUSpriteRenderLevel",
            "UserHacks_CPUCLUTRender", "UserHacks_GPUTargetCLUTMode",
            "UserHacks_AutoFlushLevel", "UserHacks_TextureInsideRt",
            "UserHacks_Limit24BitDepth",
            "UserHacks_SkipDraw_Start", "UserHacks_SkipDraw_End",
            "UserHacks_DisableDepthSupport", "UserHacks_CPU_FB_Conversion",
            "UserHacks_DisablePartialInvalidation", "paltex",
            "UserHacks_Disable_Safe_Features", "preload_frame_with_gs_data",
            "UserHacks_DisableRenderFixes", "UserHacks_ReadTCOnClose",
            "UserHacks_EstimateTextureRegion", "UserHacks_DrawBuffering",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Hardware Fixes") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsUpscalingFixesFullCatalog() {
        const QSet<QString> expected{
            "UserHacks_HalfPixelOffset", "UserHacks_native_scaling",
            "UserHacks_round_sprite_offset", "UserHacks_BilinearHack",
            "UserHacks_TCOffsetX", "UserHacks_TCOffsetY",
            "UserHacks_align_sprite_X", "UserHacks_NativePaletteDraw",
            "UserHacks_merge_pp_sprite", "UserHacks_forceEvenSpritePosition",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Upscaling Fixes") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsTextureReplacementFullCatalog() {
        // Texture search directory is RetroNest-managed and omitted.
        const QSet<QString> expected{
            "LoadTextureReplacements", "DumpReplaceableTextures",
            "LoadTextureReplacementsAsync", "DumpReplaceableMipmaps",
            "PrecacheTextureReplacements", "DumpTexturesWithFMVActive",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Texture Replacement") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsPostProcessingFullCatalog() {
        const QSet<QString> expected{
            "CASMode", "CASSharpness", "fxaa", "TVShader",
            "ShadeBoost", "ShadeBoost_Brightness", "ShadeBoost_Contrast",
            "ShadeBoost_Saturation", "ShadeBoost_Gamma",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Post-Processing") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsMediaCaptureFullCatalog() {
        // Video Recording Setup is deferred — dynamically populated codec
        // and format combos. Only screenshot settings are surfaced.
        const QSet<QString> expected{
            "ScreenshotSize", "ScreenshotFormat", "ScreenshotQuality",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Media Capture") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsAdvancedFullCatalog() {
        // Windows-only UseBlitSwapChain + ExclusiveFullscreenControl omitted.
        // FrameRateNTSC/PAL float spinboxes are deferred.
        const QSet<QString> expected{
            "HWDownloadMode", "GSDumpCompression", "texture_preloading",
            "OverrideTextureBarriers", "ExtendedUpscalingMultipliers",
            "DisableMailboxPresentation", "HWSpinCPUForReadbacks",
            "HWSpinGPUForReadbacks",
            "UseDebugDevice", "DisableShaderCache", "DisableVertexShaderExpand",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "Advanced") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testGraphicsOnScreenDisplayFullCatalog() {
        // Mirrors OSDSettingsWidget — kept as a Graphics sub-tab. OsdFontPath
        // is deferred (modal sub-dialog OsdFontPickerDialog).
        const QSet<QString> expected{
            // On-Screen Display group
            "OsdScale", "OsdMargin", "OsdMessagesPos", "OsdPerformancePos",
            "OsdBoldText",
            // Performance Stats group
            "OsdShowSpeed", "OsdShowFPS", "OsdShowVPS", "OsdShowResolution",
            "OsdShowGSStats", "OsdShowCPU", "OsdShowGPU", "OsdShowIndicators",
            "OsdShowFrameTimes",
            // System Information group
            "OsdShowHardwareInfo", "OsdShowVersion",
            // Settings & Inputs group
            "OsdShowSettings", "OsdshowPatches", "OsdShowInputs",
            "OsdShowVideoCapture", "OsdShowInputRec",
            "OsdShowTextureReplacements",
            // Messages group
            "WarnAboutUnsafeSettings",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Graphics" && d.subcategory == "On-Screen Display")
                got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testAudioCategoryFullCatalog() {
        // Audio Expansion + Stretch Settings buttons (modal sub-dialogs)
        // are deferred. DriverName/DeviceName remain hard-coded — runtime
        // enumeration is a known TODO.
        const QSet<QString> expected{
            "Backend", "DriverName", "DeviceName", "ExpansionMode",
            "SyncMode", "BufferMS", "OutputLatencyMS", "OutputLatencyMinimal",
            "StandardVolume", "FastForwardVolume", "OutputMuted",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Audio") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testMemoryCardsFullCatalog() {
        // Card-creation/conversion/delete dialogs and the drag-drop
        // MemoryCardSlotWidget are deferred — only the enable + filename
        // INI keys map to schema entries today.
        const QSet<QString> expected{
            "Slot1_Enable", "Slot1_Filename",
            "Slot2_Enable", "Slot2_Filename",
            "Multitap1_Slot2_Enable", "Multitap1_Slot3_Enable",
            "Multitap1_Slot4_Enable",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Memory Cards") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testNetworkAndHddFullCatalog() {
        // Ethernet Device Type / Device combos are deferred (dynamic OS
        // adapter list). DNS host table editor + HDD "Create Image" modal
        // are deferred too — only the static toggles + addresses surface.
        const QSet<QString> expected{
            // Ethernet
            "EthEnable",
            // Intercept DHCP
            "InterceptDHCP", "PS2IP", "AutoMask", "Mask",
            "AutoGateway", "Gateway",
            "ModeDNS1", "DNS1", "ModeDNS2", "DNS2",
            // Hard Disk Drive
            "HddEnable", "HddLBA48", "HddFile",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Network & HDD") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testAchievementsFullCatalog() {
        // Login button, profile view, audio file pickers are deferred —
        // RetroNest manages credentials and audio overrides aren't yet
        // expressible by the schema.
        const QSet<QString> expected{
            "Enabled",
            // Settings
            "ChallengeMode", "SpectatorMode", "EncoreMode", "UnofficialTestMode",
            // Notifications
            "Notifications", "NotificationsDuration",
            "LeaderboardNotifications", "LeaderboardsDuration",
            "SoundEffects", "NotificationPosition",
            // Overlay Settings
            "Overlays", "LBOverlays", "OverlayPosition",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Achievements") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testAdvancedFullCatalog() {
        // Clamping Mode is a single combo writing three INI keys via
        // saveTransform — the primary key in the schema is fpuFullMode (and
        // vu0SignOverflow / vu1SignOverflow for the VU variants).
        const QSet<QString> expected{
            // EmotionEngine (MIPS-IV)
            "FPU.Roundmode", "FPUDiv.Roundmode", "fpuFullMode",
            "EnableEE", "WaitLoop", "EnableFastmem", "EnableEECache",
            "IntcStat", "PauseOnTLBMiss", "ExtraMemory",
            // Vector Units (VU)
            "VU0.Roundmode", "vu0SignOverflow",
            "VU1.Roundmode", "vu1SignOverflow",
            "EnableVU0", "EnableVU1", "vuFlagHack", "vu1Instant",
            // I/O Processor (IOP, MIPS-I)
            "EnableIOP",
            // Game Settings
            "EnableGameFixes", "EnablePatches",
            // Savestate Settings
            "SavestateCompressionType", "SavestateCompressionRatio",
            "BackupSavestate", "SaveStateOnShutdown",
            // PINE Settings
            "EnablePINE", "PINESlot",
        };
        QSet<QString> got;
        for (const auto& d : schema_)
            if (d.category == "Advanced") got.insert(d.key);
        QCOMPARE(got, expected);
    }

    void testDebugCategoryNotInSchema() {
        // Debug pane is intentionally not surfaced — the standalone PCSX2
        // dialog's Debug tab is targeted at devs writing emulator code, not
        // end users. Removed 2026-05-05 per Mark.
        for (const auto& d : schema_)
            QVERIFY2(d.category != "Debug",
                qPrintable(QString("Debug key '%1' should not be in schema")
                           .arg(d.key)));
    }

    void testClampingModeMultiKeyTransforms() {
        // Clamping Mode (FPU) collapses three INI keys into a single combo.
        // Verify saveTransform writes all three and loadTransform reverses.
        const SettingDef* d = nullptr;
        for (const auto& def : schema_)
            if (def.category == "Advanced" && def.key == "fpuFullMode") d = &def;
        QVERIFY(d != nullptr);
        QCOMPARE(int(d->type), int(SettingDef::Combo));
        QVERIFY(d->saveTransform != nullptr);
        QVERIFY(d->loadTransform != nullptr);

        QHash<QPair<QString,QString>, QString> writes;
        auto save = [&writes](const QString& sec, const QString& k, const QString& v) {
            writes[qMakePair(sec, k)] = v;
        };
        const QString section = "EmuCore/CPU/Recompiler";

        writes.clear();
        d->saveTransform("None", save);
        QCOMPARE(writes.value({section, "fpuFullMode"}), QString("false"));
        QCOMPARE(writes.value({section, "fpuExtraOverflow"}), QString("false"));
        QCOMPARE(writes.value({section, "fpuOverflow"}), QString("false"));

        writes.clear();
        d->saveTransform("Normal (Default)", save);
        QCOMPARE(writes.value({section, "fpuOverflow"}), QString("true"));
        QCOMPARE(writes.value({section, "fpuExtraOverflow"}), QString("false"));
        QCOMPARE(writes.value({section, "fpuFullMode"}), QString("false"));

        writes.clear();
        d->saveTransform("Extra + Preserve Sign", save);
        QCOMPARE(writes.value({section, "fpuOverflow"}), QString("true"));
        QCOMPARE(writes.value({section, "fpuExtraOverflow"}), QString("true"));
        QCOMPARE(writes.value({section, "fpuFullMode"}), QString("false"));

        writes.clear();
        d->saveTransform("Full", save);
        QCOMPARE(writes.value({section, "fpuOverflow"}), QString("true"));
        QCOMPARE(writes.value({section, "fpuExtraOverflow"}), QString("true"));
        QCOMPARE(writes.value({section, "fpuFullMode"}), QString("true"));

        // loadTransform: synthesize combo value from on-disk bool combinations.
        QHash<QPair<QString,QString>, QString> store;
        auto read = [&store](const QString& sec, const QString& k) -> QString {
            return store.value({sec, k});
        };
        store = {{{section, "fpuFullMode"}, "false"},
                 {{section, "fpuExtraOverflow"}, "false"},
                 {{section, "fpuOverflow"}, "false"}};
        QCOMPARE(d->loadTransform(read), QString("None"));
        store = {{{section, "fpuFullMode"}, "false"},
                 {{section, "fpuExtraOverflow"}, "false"},
                 {{section, "fpuOverflow"}, "true"}};
        QCOMPARE(d->loadTransform(read), QString("Normal (Default)"));
        store = {{{section, "fpuFullMode"}, "false"},
                 {{section, "fpuExtraOverflow"}, "true"},
                 {{section, "fpuOverflow"}, "true"}};
        QCOMPARE(d->loadTransform(read), QString("Extra + Preserve Sign"));
        store = {{{section, "fpuFullMode"}, "true"},
                 {{section, "fpuExtraOverflow"}, "true"},
                 {{section, "fpuOverflow"}, "true"}};
        QCOMPARE(d->loadTransform(read), QString("Full"));
    }

    void testBoolValuesAreLowercaseTrueFalse() {
        // PCSX2 writes bools via StringUtil::BoolToString as lowercase
        // "true"/"false" (common/StringUtil.h:199).
        for (const auto& d : schema_) {
            if (d.type != SettingDef::Bool) continue;
            QVERIFY2(d.defaultValue == "true" || d.defaultValue == "false",
                     qPrintable(QString("Bool '%1' has non-canonical default '%2'")
                                .arg(d.key, d.defaultValue)));
        }
    }
};

QTEST_MAIN(TestPcsx2Schema)
#include "test_pcsx2_schema.moc"
