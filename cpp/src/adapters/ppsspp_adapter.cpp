#include "ppsspp_adapter.h"
#include "core/sfo_parser.h"
#include "core/iso9660_reader.h"
#include "core/ini_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>

static const char* PPSSPP_INSTALL_FOLDER = "ppsspp";

// ============================================================================
// Platform-specific config directory
// ============================================================================

QString PPSSPPAdapter::configDir() {
    // Portable memstick root — PPSSPP reads config from {memstick}/PSP/SYSTEM/
    return Paths::emulatorsDir(PPSSPP_INSTALL_FOLDER);
}

QString PPSSPPAdapter::nativeConfigDir() {
    // PPSSPP's config lives at {memstick}/PSP/SYSTEM/
    return configDir() + "/PSP/SYSTEM";
}

QString PPSSPPAdapter::iniPath() {
    // Config file that PPSSPP reads. Our settings UI also reads/writes this
    // file directly, so changes are instantly reflected in both UIs.
    return nativeConfigDir() + "/ppsspp.ini";
}

QString PPSSPPAdapter::controlsIniPath() {
    return nativeConfigDir() + "/controls.ini";
}

QString PPSSPPAdapter::configFilePath() const {
    return iniPath();
}

QString PPSSPPAdapter::controllerBindingsConfigFilePath() const {
    // PPSSPP stores controller bindings in a separate controls.ini file
    return controlsIniPath();
}

QString PPSSPPAdapter::controllerBindingsSection(int /*port*/) const {
    // PPSSPP uses a single [ControlMapping] section regardless of port
    return "ControlMapping";
}

// ============================================================================
// previewSpec — declare which preview widget the Recommended and Graphics →
// On-Screen Display sub-tab pages render alongside their cards. Mirrors the
// Dolphin / PCSX2 / DuckStation pattern.
//
// Recommended → AspectRatioPreview is decorative-only (no schema key bound):
// PPSSPP's standalone UI exposes no discrete aspect-ratio enum at this
// level. The preview renders its default 4:3 frame and stays static.
// GenericSettingsPage falls back to the first group as primaryPreviewGroup
// so cards still lay out beside the preview.
//
// Graphics → On-Screen Display → OsdPreview binds individual bits of the
// packed-int iShowStatusFlags key to the matching OsdPreview Q_PROPERTY
// using the "<key>:<bitmask>" lookup syntax — see wirePreviewBinding.
// Show Battery % has no OsdPreview equivalent (only FPS / Speed / battery
// in PPSSPP's bitmask, vs OsdPreview's many show* properties), so its
// toggle stays unbound.
// ============================================================================

PreviewSpec PPSSPPAdapter::previewSpec(const QString& category,
                                       const QString& subcategory) const {
    if (category == "Recommended" && subcategory.isEmpty()) {
        return {"aspect", {}};
    }
    if (category == "Graphics" && subcategory == "On-Screen Display") {
        return {"osd", {
            {"iShowStatusFlags:2", "showFps"},     // FPS_COUNTER     bit
            {"iShowStatusFlags:4", "showSpeed"},   // SPEED_COUNTER   bit
        }};
    }
    return {};
}

QString PPSSPPAdapter::subcategoryIcon(const QString& category,
                                        const QString& subcategory) const {
    if (category != "Graphics") return {};
    if (subcategory == "General")            return QStringLiteral("\U0001F5BC");  // 🖼
    if (subcategory == "On-Screen Display")  return QStringLiteral("\U0001F4CA");  // 📊
    return {};
}

// ============================================================================
// Settings schema
// ============================================================================
//
// Mirrors upstream PPSSPP's GameSettingsScreen panes verbatim — same top-level
// tabs, same group order, same setting order, same labels, same gating chains.
// Reference: references/ppsspp-master/UI/GameSettingsScreen.cpp.
//
// Top-level tabs:  Recommended · Graphics · Audio · Networking · System
//
// "Recommended" is a curated VIEW of the most-tweaked settings, prepended at
// the top of the dialog — same pattern as Dolphin / PCSX2 / DuckStation. Its
// entries bind the same INI keys as the equivalent rows in Graphics / Audio /
// System, so editing through either path produces identical INI output. Not
// part of upstream PPSSPP's UI.
//
// Deliberately NOT exposed (with rationale):
// - Controls tab: handled by separate controller_mapping_page / hotkey UI.
// - Tools tab: every entry is a navigation submodal (RetroAchievements,
//   Savedata Manager, System Information, Developer Tools, Remote disc
//   streaming) — no direct INI-backed settings to render. DebugOverlay
//   (a real INI key) lives inside the Developer Tools submodal upstream;
//   it's deferred until we have submodal-rendering infra.
// - VR tab: gated on DEVICE_TYPE_VR upstream; not relevant to macOS Dolphin/
//   PPSSPP emulation use.
// - Display tab "Full screen" toggle: embedding-critical (force-patched true).
// - "Pause when not focused" (System → General): embedding-critical
//   (force-patched true so our overlay can take over pausing).
// - Settings backed by PopupTextInputChoice / PopupTextInputChoice with text
//   restrictions: Nickname, Username, DNS server, Quick chat 1-5. Our renderer
//   has no text-input widget yet (deferral: see ppsspp-schema-alignment).
// - Float sliders (fUITint, fUISaturation, fAnalogTriggerThreshold, etc.):
//   integer-only slider widget, deferred.
// - Submodal-driven settings: GPU texture upscaler (TextureShaderScreen),
//   Theme (filesystem-scanned combo), Language (NewLanguageScreen),
//   Set Memory Stick folder, Display layout & effects (DisplayLayoutScreen,
//   which hosts the post-processing shader picker).
// - Compile-gated upstream on macOS SDL build:
//   * Discord Rich Presence (ENABLE_DISCORD)
//   * Cardboard VR group (Android/iOS)
//   * App switching mode (iOS)
//   * Hide navigation bar (Android)
//   * Bluetooth-friendly buffer (Android)
//   * Recording group (Windows or Qt UI build, not SDL)
//   * iAndroidHwScale, iAppSwitchMode

QVector<SettingDef> PPSSPPAdapter::settingsSchema() const {
    QVector<SettingDef> s;

    // ────────────────────────────────────────────────────────────────────────
    // RECOMMENDED — curated short list of the settings most users actually
    // tweak, grouped by impact. Mirrors the Dolphin / PCSX2 / DuckStation
    // Recommended pattern (see `recommended-page-layout-contract.md` in user
    // memory). Every entry binds the same INI section/key as its full-pane
    // counterpart, so editing here and editing in the matching Graphics /
    // Audio / System pane produce identical results.
    //
    // The AspectRatioPreview shown beside this category's cards is purely
    // decorative — PPSSPP's standalone UI doesn't expose a discrete
    // aspect-ratio enum at this level (it lives in the deferred
    // DisplayLayoutScreen submodal), so the schema doesn't bind any key
    // to the preview. The preview renders its default 4:3 frame and stays
    // static while the user tweaks settings; layout still matches the
    // other emulators because GenericSettingsPage falls back to the first
    // group in the subcategory when no key is bound.
    // ────────────────────────────────────────────────────────────────────────

    // Performance — biggest impact for getting games playable.
    s.append({"Recommended", "", "Performance", "Graphics", "GraphicsBackend", "Backend",
              "Graphics API used for rendering. Vulkan is the macOS default; "
              "OpenGL is a fallback for older drivers. Switching backends "
              "often produces the single biggest performance change.",
              SettingDef::Combo, "3 (VULKAN)",
              {{"OpenGL", "0 (OPENGL)"},
#if defined(Q_OS_WIN)
               {"Direct3D 11", "2 (DIRECT3D11)"},
#endif
               {"Vulkan", "3 (VULKAN)"}}, 0, 0, 0});
    s.append({"Recommended", "", "Performance", "Graphics", "SkipBufferEffects", "Skip Buffer Effects",
              "Faster, but graphics may be missing in some games. Big speed-up "
              "for games that don't rely on buffer effects.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Recommended", "", "Performance", "Graphics", "AutoFrameSkip", "Auto FrameSkip",
              "Automatically skip frames to maintain full speed when the host "
              "can't keep up. Recommended for low-end hardware.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Recommended", "", "Performance", "Graphics", "InflightFrames", "Buffer graphics commands",
              "Faster, but adds input lag. Up to 1 is a good balance.",
              SettingDef::Combo, "1",
              {{"No buffer", "0"}, {"Up to 1", "1"}, {"Up to 2", "2"}}, 0, 0, 0});

    // Visual Quality — what users see, in order of typical impact.
    s.append({"Recommended", "", "Visual Quality", "Graphics", "InternalResolution", "Rendering Resolution",
              "Higher multipliers sharpen the PSP's 480x272 framebuffer at the "
              "cost of GPU load. 2x or 3x is enough for most modern displays.",
              SettingDef::Combo, "0",
              {{"Auto (1:1)", "0"}, {"1x PSP", "1"}, {"2x PSP", "2"},
               {"3x PSP", "3"}, {"4x PSP", "4"}, {"5x PSP", "5"},
               {"6x PSP", "6"}, {"7x PSP", "7"}, {"8x PSP", "8"},
               {"9x PSP", "9"}, {"10x PSP", "10"}}, 0, 0, 0});
    s.append({"Recommended", "", "Visual Quality", "Graphics", "MultiSampleLevel", "Antialiasing (MSAA)",
              "Smooths jagged edges. 2x or 4x is a good balance; higher values "
              "scale GPU cost faster than visual return.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"}, {"16x", "4"}}, 0, 0, 0});
    s.append({"Recommended", "", "Visual Quality", "Graphics", "TextureFiltering", "Texture Filter",
              "How textures are sampled. Auto Max Quality looks best but costs "
              "fillrate; Linear is the safe default.",
              SettingDef::Combo, "1",
              {{"Auto", "1"}, {"Nearest", "2"}, {"Linear", "3"}, {"Auto Max Quality", "4"}}, 0, 0, 0});
    s.append({"Recommended", "", "Visual Quality", "Graphics", "AnisotropyLevel", "Anisotropic Filtering",
              "Sharpens textures viewed at oblique angles. 4x or 8x is ideal "
              "for most games — the cost is small on modern GPUs.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"}, {"16x", "4"}}, 0, 0, 0});

    // Frame Pacing — smoothness knobs.
    s.append({"Recommended", "", "Frame Pacing", "Graphics", "VerticalSync", "VSync",
              "Synchronize rendering to display refresh rate. Eliminates "
              "tearing at the cost of a frame of latency.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Recommended", "", "Frame Pacing", "Graphics", "FrameSkip", "Frame Skipping",
              "Manually skip N rendered frames per game frame. Use only if "
              "Auto FrameSkip can't hold full speed.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"1", "1"}, {"2", "2"}, {"3", "3"},
               {"4", "4"}, {"5", "5"}, {"6", "6"}, {"7", "7"}, {"8", "8"}}, 0, 0, 0});
    s.append({"Recommended", "", "Frame Pacing", "Graphics", "FrameRate", "Alternative speed",
              "Speed used when the alternative-speed hotkey is held. Stored "
              "as raw FPS — labels show the equivalent percent of 60 FPS.",
              SettingDef::Combo, "0",
              {{"Unlimited (No Cap)", "0"},
               {"50% (30 FPS)",  "30"},
               {"75% (45 FPS)",  "45"},
               {"100% (60 FPS)", "60"},
               {"125% (75 FPS)", "75"},
               {"150% (90 FPS)", "90"},
               {"200% (120 FPS)","120"},
               {"300% (180 FPS)","180"},
               {"500% (300 FPS)","300"}}, 0, 0, 0});

    // Audio — the two knobs that matter for headless emulation.
    s.append({"Recommended", "", "Audio", "Sound", "Enable", "Enable Sound",
              "Master audio toggle. Off only for silent benchmarking.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Recommended", "", "Audio", "Sound", "AudioSyncMode", "Playback mode",
              "Smooth reduces audio artifacts at the cost of slightly higher "
              "latency. Classic is the lowest-latency option.",
              SettingDef::Combo, "1",
              {{"Smooth (reduces artifacts)", "0"}, {"Classic (lowest latency)", "1"}}, 0, 0, 0});
    s.append({"Recommended", "", "Audio", "Sound", "GameVolume", "Game volume",
              "Master in-game volume.",
              SettingDef::Int, "100", {}, 0, 100, 5, "slider", "%"});

    // Convenience — quality-of-life toggles that aren't visual or audio.
    s.append({"Recommended", "", "Convenience", "General", "AutoLoadSaveState", "Auto load savestate",
              "Automatically load a save state when launching a game. Slot 1 "
              "is a common pick for quick resume.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"Newest Save", "2"},
               {"Slot 1", "3"}, {"Slot 2", "4"}, {"Slot 3", "5"},
               {"Slot 4", "6"}, {"Slot 5", "7"}}, 0, 0, 0});
    s.append({"Recommended", "", "Convenience", "General", "EnableCheats", "Enable Cheats",
              "Apply cheat codes loaded for the running game.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ────────────────────────────────────────────────────────────────────────
    // GRAPHICS — two sub-tabs: General (mirrors upstream's
    // CreateGraphicsSettings ItemHeader sequence verbatim) + On-Screen
    // Display (the bitmask Show* toggles, hosting OsdPreview to match the
    // other adapters' OSD sub-tabs). Upstream PPSSPP renders all of this
    // as one scrolling page; we split for parity with Dolphin / PCSX2 /
    // DuckStation which all carve OSD off into its own sub-tab beside an
    // OsdPreview.
    // ────────────────────────────────────────────────────────────────────────

    // ── Group: Rendering Mode ──
    // Backend: stored as "<int> (<NAME>)" via GPUBackendTranslator.
    s.append({"Graphics", "General","Rendering Mode", "Graphics", "GraphicsBackend", "Backend",
              "Graphics API used for rendering.",
              SettingDef::Combo, "3 (VULKAN)",
              {{"OpenGL", "0 (OPENGL)"},
#if defined(Q_OS_WIN)
               {"Direct3D 11", "2 (DIRECT3D11)"},
#endif
               {"Vulkan", "3 (VULKAN)"}}, 0, 0, 0});
    s.append({"Graphics", "General","Rendering Mode", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "0",
              {{"Auto (1:1)", "0"}, {"1x PSP", "1"}, {"2x PSP", "2"},
               {"3x PSP", "3"}, {"4x PSP", "4"}, {"5x PSP", "5"},
               {"6x PSP", "6"}, {"7x PSP", "7"}, {"8x PSP", "8"},
               {"9x PSP", "9"}, {"10x PSP", "10"}}, 0, 0, 0,
              "", "", "!SoftwareRenderer && !SkipBufferEffects"});
    s.append({"Graphics", "General","Rendering Mode", "Graphics", "SoftwareRenderer", "Software Rendering (slow)",
              "Uses CPU rendering for maximum accuracy. Very slow.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Graphics", "General","Rendering Mode", "Graphics", "MultiSampleLevel", "Antialiasing (MSAA)",
              "Multisample anti-aliasing level.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"}, {"16x", "4"}},
              0, 0, 0, "", "", "!SoftwareRenderer && !SkipBufferEffects"});
    s.append({"Graphics", "General","Rendering Mode", "Graphics", "ReplaceTextures", "Replace textures",
              "Allow custom texture replacement packs.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ── Group: Display ──
    // Full screen / Display layout & effects intentionally omitted (see header).
    s.append({"Graphics", "General","Display", "Graphics", "VerticalSync", "VSync",
              "Synchronize rendering to display refresh rate.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Graphics", "General","Display", "Graphics", "LowLatencyPresent", "Low latency display",
              "Reduce display latency where the backend supports MAILBOX present mode.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "VerticalSync"});

    // ── Group: Frame Rate Control ──
    // Frame Skipping is gated on !AutoFrameSkip upstream.
    s.append({"Graphics", "General","Frame Rate Control", "Graphics", "FrameSkip", "Frame Skipping",
              "Number of frames to skip to maintain speed.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"1", "1"}, {"2", "2"}, {"3", "3"},
               {"4", "4"}, {"5", "5"}, {"6", "6"}, {"7", "7"}, {"8", "8"}},
              0, 0, 0, "", "", "!AutoFrameSkip"});
    s.append({"Graphics", "General","Frame Rate Control", "Graphics", "AutoFrameSkip", "Auto FrameSkip",
              "Automatically skip frames to maintain speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    // iFpsLimit1 / iFpsLimit2 are stored as raw FPS, not percent — upstream
    // converts (percent * 60) / 100 outside the INI layer. Pre-bake the FPS
    // values into combo INI values so percentage-labelled rows still round-
    // trip. See audit 2026-04-07. Upstream presents these as continuous
    // sliders with zero/negative-disable chrome we don't have yet — combo
    // is the deferred-chrome workaround.
    s.append({"Graphics", "General","Frame Rate Control", "Graphics", "FrameRate", "Alternative speed",
              "Speed used when the alternative-speed hotkey is held.",
              SettingDef::Combo, "0",
              {{"Unlimited (No Cap)", "0"},
               {"25% (15 FPS)",  "15"},
               {"50% (30 FPS)",  "30"},
               {"75% (45 FPS)",  "45"},
               {"100% (60 FPS)", "60"},
               {"125% (75 FPS)", "75"},
               {"150% (90 FPS)", "90"},
               {"200% (120 FPS)","120"},
               {"300% (180 FPS)","180"},
               {"500% (300 FPS)","300"}}, 0, 0, 0});
    s.append({"Graphics", "General","Frame Rate Control", "Graphics", "FrameRate2", "Alternative speed 2",
              "Second alternative speed for toggling. Same FPS-vs-percent caveat as above.",
              SettingDef::Combo, "-1",
              {{"Disabled",         "-1"},
               {"Unlimited (No Cap)", "0"},
               {"25% (15 FPS)",  "15"},
               {"50% (30 FPS)",  "30"},
               {"75% (45 FPS)",  "45"},
               {"100% (60 FPS)", "60"},
               {"125% (75 FPS)", "75"},
               {"150% (90 FPS)", "90"},
               {"200% (120 FPS)","120"},
               {"300% (180 FPS)","180"},
               {"500% (300 FPS)","300"}}, 0, 0, 0});

    // ── Group: Speed Hacks ──
    s.append({"Graphics", "General","Speed Hacks", "Graphics", "SkipBufferEffects", "Skip Buffer Effects",
              "Faster, but graphics may be missing in some games.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Speed Hacks", "Graphics", "DisableRangeCulling", "Disable culling",
              "Disables range culling.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Speed Hacks", "Graphics", "SkipGPUReadbackMode", "Skip GPU Readbacks",
              "Skipping GPU readbacks is faster but may break some games.",
              SettingDef::Combo, "0",
              {{"No", "0"}, {"Skip", "1"}, {"Copy to texture", "2"}}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Speed Hacks", "Graphics", "DepthRasterMode", "Lens flare occlusion",
              "Controls how the depth raster is used for lens flare occlusion.",
              SettingDef::Combo, "0",
              {{"Auto", "0"}, {"Low", "1"}, {"Off", "2"}, {"Always on", "3"}},
              0, 0, 0, "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Speed Hacks", "Graphics", "TextureBackoffCache", "Lazy texture caching (speedup)",
              "Faster, but can cause text problems in a few games.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Speed Hacks", "Graphics", "SplineBezierQuality", "Spline/Bezier curves quality",
              "Only used by some games, controls smoothness of curves.",
              SettingDef::Combo, "2",
              {{"Low", "0"}, {"Medium", "1"}, {"High", "2"}}, 0, 0, 0});
    s.append({"Graphics", "General","Speed Hacks", "Graphics", "BloomHack", "Lower resolution for effects",
              "Reduces artifacts.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"Safe", "1"}, {"Balanced", "2"}, {"Aggressive", "3"}},
              0, 0, 0, "", "",
              "!SoftwareRenderer && InternalResolution!=1"});

    // ── Group: Performance ──
    s.append({"Graphics", "General","Performance", "Graphics", "RenderDuplicateFrames", "Render duplicate frames to 60hz",
              "Can make framerate smoother in games that run at lower framerates.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "!SkipBufferEffects && FrameSkip=0"});
    s.append({"Graphics", "General","Performance", "Graphics", "InflightFrames", "Buffer graphics commands",
              "Faster, but adds input lag.",
              SettingDef::Combo, "1",
              {{"No buffer", "0"}, {"Up to 1", "1"}, {"Up to 2", "2"}}, 0, 0, 0});
    s.append({"Graphics", "General","Performance", "Graphics", "HardwareTransform", "Hardware Transform",
              "Uses hardware geometry transformation. Disable only for debugging.",
              SettingDef::Bool, "true", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Performance", "Graphics", "SoftwareSkinning", "Software Skinning",
              "Combine skinned model draws on the CPU, faster in most games.",
              SettingDef::Bool, "true", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Performance", "Graphics", "HardwareTessellation", "Hardware Tessellation",
              "Uses hardware to make curves.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer && HardwareTransform"});

    // ── Group: Texture upscaling ──
    // GPU texture upscaler (sTextureShaderName) — submodal TextureShaderScreen
    // upstream, deferred until we have submodal-rendering infra.
    s.append({"Graphics", "General","Texture upscaling", "Graphics", "TexScalingType", "CPU texture upscaler (slow)",
              "Algorithm used for texture upscaling.",
              SettingDef::Combo, "0",
              {{"xBRZ", "0"}, {"Hybrid", "1"}, {"Bicubic", "2"}, {"Hybrid + Bicubic", "3"}},
              0, 0, 0, "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Texture upscaling", "Graphics", "TexScalingLevel", "Upscale Level",
              "CPU heavy - some scaling may be delayed to avoid stutter.",
              SettingDef::Combo, "1",
              {{"Off", "1"}, {"2x", "2"}, {"3x", "3"}, {"4x", "4"}, {"5x", "5"}},
              0, 0, 0, "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Texture upscaling", "Graphics", "TexDeposterize", "Deposterize",
              "Fixes visual banding glitches in upscaled textures.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});

    // ── Group: Texture Filtering ──
    s.append({"Graphics", "General","Texture Filtering", "Graphics", "AnisotropyLevel", "Anisotropic Filtering",
              "Improves texture quality at oblique angles.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"2x", "1"}, {"4x", "2"}, {"8x", "3"}, {"16x", "4"}},
              0, 0, 0, "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Texture Filtering", "Graphics", "TextureFiltering", "Texture Filter",
              "Filtering applied to textures.",
              SettingDef::Combo, "1",
              {{"Auto", "1"}, {"Nearest", "2"}, {"Linear", "3"}, {"Auto Max Quality", "4"}},
              0, 0, 0, "", "", "!SoftwareRenderer"});
    s.append({"Graphics", "General","Texture Filtering", "Graphics", "Smart2DTexFiltering", "Smart 2D texture filtering",
              "Smarter filtering for 2D textures.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "!SoftwareRenderer"});

    // ── Group: Overlay Information ──
    // iShowStatusFlags packs FPS_COUNTER(2), SPEED_COUNTER(4), BATTERY_PERCENT(8)
    // into one int. PPSSPP's own default is 0 (verified Core/Config.cpp).
    s.append({"Graphics", "On-Screen Display", "Overlay Information", "Graphics", "iShowStatusFlags","Show FPS Counter",
              "Display the framerate counter in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", /*bitmask=*/2});
    s.append({"Graphics", "On-Screen Display", "Overlay Information", "Graphics", "iShowStatusFlags","Show Speed",
              "Display the emulation speed percentage in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", /*bitmask=*/4});
    s.append({"Graphics", "On-Screen Display", "Overlay Information", "Graphics", "iShowStatusFlags","Show Battery %",
              "Display the host battery percentage in-game.",
              SettingDef::Bool, "0", {}, 0, 0, 0, "", "", "", /*bitmask=*/8});
    // Debug overlay — upstream's Graphics tab adds this via AddOverlayList()
    // right after the BitCheckBoxes (UI/GameSettingsScreen.cpp:651). Combo
    // labels mirror the upstream g_debugOverlayList strings; "Frame profile"
    // (index 4) is gated on USE_PROFILER upstream so we omit it for build-
    // flag safety (the surrounding indices stay stable). iDebugOverlay is
    // marked DONT_SAVE in PPSSPP's own config — our ensureConfig patches it
    // before each launch so the value lives across launches as long as the
    // user changes it through our UI.
    s.append({"Graphics", "On-Screen Display", "Overlay Information", "General", "DebugOverlay", "Debug overlay",
              "PPSSPP debug overlay. PPSSPP doesn't persist this setting "
              "between runs, so launching the emulator outside this app "
              "resets it to Off.",
              SettingDef::Combo, "0",
              {{"Off", "0"},
               {"Debug stats", "1"},
               {"Draw Frametimes Graph", "2"},
               {"Frame timing", "3"},
               {"Control Debug", "5"},
               {"Audio Debug", "6"},
               {"GPU Profile", "7"},
               {"GPU Allocator Viewer", "8"},
               {"Framebuffer list", "9"}}, 0, 0, 0});

    // ── Group: Notifications ──
    // iNotificationPos is in upstream's System → UI section, but it's
    // structurally an OSD setting (controls where in-game notifications
    // appear) and the other adapters all surface notification position on
    // their OSD pages. Hosting it here matches that cross-emulator pattern;
    // the underlying INI section/key is unchanged so round-trip stays
    // identical regardless of which page the user edits it from.
    s.append({"Graphics", "On-Screen Display", "Notifications", "General", "NotificationPos", "Notification screen position",
              "Where in-game notifications appear over the game.",
              SettingDef::Combo, "4",
              {{"None", "-1"}, {"Bottom Left", "0"}, {"Bottom Center", "1"},
               {"Bottom Right", "2"}, {"Top Left", "3"}, {"Top Center", "4"},
               {"Top Right", "5"}, {"Center Left", "6"}, {"Center Right", "7"}},
              0, 0, 0});

    // ────────────────────────────────────────────────────────────────────────
    // AUDIO — mirrors CreateAudioSettings (UI/GameSettingsScreen.cpp:654).
    // ────────────────────────────────────────────────────────────────────────

    // ── Group: Audio playback ──
    s.append({"Audio", "", "Audio playback", "Sound", "AudioSyncMode", "Playback mode",
              "Audio synchronization method.",
              SettingDef::Combo, "1",
              {{"Smooth (reduces artifacts)", "0"}, {"Classic (lowest latency)", "1"}}, 0, 0, 0});
    s.append({"Audio", "", "Audio playback", "Sound", "FillAudioGaps", "Fill audio gaps",
              "Fill gaps in audio output to prevent pops.",
              SettingDef::Bool, "true", {}, 0, 0, 0,
              "", "", "AudioSyncMode=0"});

    // ── Group: Game volume ──
    s.append({"Audio", "", "Game volume", "Sound", "Enable", "Enable Sound",
              "Enable audio output.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"Audio", "", "Game volume", "Sound", "GameVolume", "Game volume",
              "Master audio volume.",
              SettingDef::Int, "100", {}, 0, 100, 5, "slider", "%",
              "Enable"});
    s.append({"Audio", "", "Game volume", "Sound", "ReverbRelativeVolume", "Reverb volume",
              "Volume of reverb effects.",
              SettingDef::Int, "100", {}, 0, 200, 5, "slider", "%",
              "Enable"});
    s.append({"Audio", "", "Game volume", "Sound", "AltSpeedRelativeVolume", "Alternate speed volume",
              "Volume when using fast-forward.",
              SettingDef::Int, "100", {}, 0, 100, 5, "slider", "%",
              "Enable"});
    s.append({"Audio", "", "Game volume", "Sound", "AchievementVolume", "Achievement sound volume",
              "Volume of achievement notification sounds.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%",
              "Enable"});

    // ── Group: UI sound ──
    // bUISound is in [General], iUIVolume / iGamePreviewVolume in [Sound].
    s.append({"Audio", "", "UI sound", "General", "UISound", "UI sound",
              "Play sounds for UI interactions.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Audio", "", "UI sound", "Sound", "UIVolume", "UI volume",
              "Volume of UI sounds.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%",
              "UISound"});
    s.append({"Audio", "", "UI sound", "Sound", "GamePreviewVolume", "Game preview volume",
              "Volume of game previews in the UI.",
              SettingDef::Int, "75", {}, 0, 100, 5, "slider", "%"});

    // ── Group: Audio backend ──
    // SDL build path. iSDLAudioBufferSize is restricted to {128,256,512,1024,
    // 2048} upstream. Buffer size is below 128 silently clamped to 128 by the
    // SDL backend, so minVal=128 matches the actual clamp.
    s.append({"Audio", "", "Audio backend", "Sound", "AudioBufferSize", "Buffer size",
              "Audio buffer size in samples. Smaller = less latency but more crackling risk.",
              SettingDef::Int, "256", {}, 128, 2048, 64, "slider", ""});
    s.append({"Audio", "", "Audio backend", "Sound", "AutoAudioDevice", "Use new audio devices automatically",
              "Automatically switch to newly connected audio devices.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ────────────────────────────────────────────────────────────────────────
    // NETWORKING — mirrors CreateNetworkingSettings (line 1011). Text-input
    // settings (Nickname, Username, DNS server, Quick chat 1-5, MAC address,
    // Ad hoc server address) are deferred — see header.
    // ────────────────────────────────────────────────────────────────────────

    // ── Group: Networking (top-level header in upstream) ──
    s.append({"Networking", "", "Networking", "Network", "EnableWlan", "Enable networking/wlan (beta)",
              "Enable PSP wireless networking emulation.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ── Group: Ad Hoc multiplayer ──
    // Ad hoc server address (sProAdhocServer) deferred — submodal AdhocServerScreen.
    // Nickname (sNickName) deferred — text input.
    s.append({"Networking", "", "Ad Hoc multiplayer", "Network", "AdhocServerRelayMode", "Try to use server-provided packet relay",
              "Available on servers that provide 'aemu_postoffice' packet relay (e.g. socom.cc). Disable for LAN/VPN play.",
              SettingDef::Combo, "0",
              {{"Auto", "0"}, {"Yes", "1"}, {"No", "2"}}, 0, 0, 0});
    s.append({"Networking", "", "Ad Hoc multiplayer", "Network", "EnableAdhocServer", "Enable built-in ad hoc server",
              "Run an ad hoc server inside PPSSPP for local multiplayer.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ── Group: Infrastructure ──
    // Username (sInfrastructureUsername) deferred — text input.
    // DNS server (sInfrastructureDNSServer) deferred — text input.
    s.append({"Networking", "", "Infrastructure", "Network", "InfrastructureAutoDNS", "Autoconfigure",
              "Auto-configure DNS for infrastructure-mode multiplayer.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ── Group: UPnP (port-forwarding) ──
    s.append({"Networking", "", "UPnP (port-forwarding)", "Network", "EnableUPnP", "Enable UPnP (need a few seconds to detect)",
              "Use UPnP to auto-configure router port forwarding.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Networking", "", "UPnP (port-forwarding)", "Network", "UPnPUseOriginalPort", "UPnP use original port (Enabled = PSP compatibility)",
              "May not work for all devices or games — see wiki.",
              SettingDef::Bool, "false", {}, 0, 0, 0,
              "", "", "EnableUPnP"});

    // ── Group: Chat ──
    s.append({"Networking", "", "Chat", "Network", "EnableNetworkChat", "Enable network chat",
              "Allow text chat with other players.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Networking", "", "Chat", "Network", "ChatButtonPosition", "Chat Button Position",
              "Where the in-game chat button is displayed.",
              SettingDef::Combo, "0",
              {{"Bottom Left", "0"}, {"Bottom Center", "1"}, {"Bottom Right", "2"},
               {"Top Left", "3"}, {"Top Center", "4"}, {"Top Right", "5"},
               {"Center Left", "6"}, {"Center Right", "7"}, {"None", "8"}},
              0, 0, 0, "", "", "EnableNetworkChat"});
    s.append({"Networking", "", "Chat", "Network", "ChatScreenPosition", "Chat Screen Position",
              "Where the chat overlay window is anchored.",
              SettingDef::Combo, "0",
              {{"Bottom Left", "0"}, {"Bottom Center", "1"}, {"Bottom Right", "2"},
               {"Top Left", "3"}, {"Top Center", "4"}, {"Top Right", "5"}},
              0, 0, 0, "", "", "EnableNetworkChat"});

    // ── Group: Quick chat ──
    // Quick chat 1-5 individual text inputs deferred.
    s.append({"Networking", "", "Quick chat", "Network", "EnableQuickChat", "Enable quick chat",
              "Show one-tap quick chat buttons.",
              SettingDef::Bool, "true", {}, 0, 0, 0,
              "", "", "EnableNetworkChat"});

    // ── Group: Misc ──
    // WLAN Channel: upstream HideChoice(2-5) and HideChoice(7-10) — only
    // Auto / 1 / 6 / 11 are reachable. Match that in our combo entries.
    s.append({"Networking", "", "Misc (default = compatibility)", "Network", "WlanAdhocChannel", "WLAN Channel",
              "PSP-side ad hoc WLAN channel.",
              SettingDef::Combo, "0",
              {{"Auto", "0"}, {"1", "1"}, {"6", "6"}, {"11", "11"}}, 0, 0, 0});
    s.append({"Networking", "", "Misc (default = compatibility)", "Network", "PortOffset", "Port offset",
              "Offset added to all multiplayer ports.",
              SettingDef::Int, "10000", {}, 0, 60000, 100, "slider", ""});
    s.append({"Networking", "", "Misc (default = compatibility)", "Network", "MinTimeout", "Minimum Timeout (override in ms, 0 = default)",
              "Minimum network timeout override.",
              SettingDef::Int, "0", {}, 0, 15000, 50, "slider", "ms"});
    s.append({"Networking", "", "Misc (default = compatibility)", "Network", "ForcedFirstConnect", "Forced First Connect (faster Connect)",
              "Skip the initial connect handshake when joining ad hoc games.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"Networking", "", "Misc (default = compatibility)", "Network", "AllowSpeedControlWhileConnected", "Allow speed control while connected (not recommended)",
              "Enable fast-forward / slow-mo even while connected to other players.",
              SettingDef::Bool, "false", {}, 0, 0, 0});

    // ────────────────────────────────────────────────────────────────────────
    // SYSTEM — mirrors CreateSystemSettings (line 1173). Several groups
    // appear here only because upstream's pane is structured this way:
    // Emulation, Save states, Cheats, PSP Settings all live under System
    // even though we used to surface "Emulation" as a top-level tab.
    // ────────────────────────────────────────────────────────────────────────

    // ── Group: UI ──
    // Language, Theme, Color tint/saturation, Set/Clear UI background,
    // App switching mode, Hide navigation bar all deferred — see header.
    s.append({"System", "", "UI", "General", "UIScaleFactor", "UI size adjustment (DPI)",
              "Adjust the in-emulator UI scale. 0 = automatic.",
              SettingDef::Int, "0", {}, -8, 8, 1, "slider", ""});
    s.append({"System", "", "UI", "General", "TransparentBackground", "Transparent UI background",
              "Use a translucent background for the PPSSPP menu UI.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    // iNotificationPos lives on Graphics → On-Screen Display now (matches
    // the other adapters' OSD-page placement of notification position).
    s.append({"System", "", "UI", "General", "BackgroundAnimation", "UI background animation",
              "Animation behind PPSSPP's menu UI.",
              SettingDef::Combo, "1",
              {{"No animation", "0"}, {"Floating symbols", "1"},
               {"Recent games", "2"}, {"Waves", "3"}, {"Moving background", "4"},
               {"Bouncing icon", "5"}, {"Colored floating symbols", "6"}},
              0, 0, 0});

    // ── Group: PSP Memory Stick ──
    // Show / Set Memory Stick folder and Windows-only "in My Documents"
    // toggles deferred — submodal navigation.
    s.append({"System", "", "PSP Memory Stick", "General", "MemStickInserted", "Memory Stick inserted",
              "Whether the emulated PSP memory stick is mounted.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"System", "", "PSP Memory Stick", "SystemParam", "MemStickSize", "Memory Stick size",
              "Reported memory stick capacity in GB.",
              SettingDef::Int, "16", {}, 1, 32, 1, "slider", "GB"});

    // ── Group: Emulation ──
    // FastMemoryAccess + IOTimingMethod + CPUSpeed live in [CPU] per
    // Core/Config.cpp::cpuSettings[]; IgnoreBadMemAccess + ForceLagSync2
    // live in [General] per generalSettings[].
    s.append({"System", "", "Emulation", "CPU", "FastMemoryAccess", "Fast Memory",
              "Uses faster but less accurate memory access. May cause crashes in some games.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"System", "", "Emulation", "General", "IgnoreBadMemAccess", "Ignore bad memory accesses",
              "Silently ignores invalid memory reads/writes instead of crashing.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"System", "", "Emulation", "CPU", "IOTimingMethod", "I/O timing method",
              "Controls how UMD (disc) I/O timing is handled.",
              SettingDef::Combo, "0",
              {{"Fast (lag on slow storage)", "0"},
               {"Host (bugs, less lag)", "1"},
               {"Simulate UMD delays", "2"},
               {"Simulate UMD slow reading speed", "3"}}, 0, 0, 0});
    // ForceLagSync2 is gated on !AutoFrameSkip upstream — but AutoFrameSkip
    // lives on the Graphics tab. dependsOn DSL only resolves keys within the
    // same category, so the gate is informational here. Keep it visible.
    s.append({"System", "", "Emulation", "General", "ForceLagSync2", "Force real clock sync (slower, less lag)",
              "Slower but less lag. Forces the emulator to run at real clock speed.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"System", "", "Emulation", "CPU", "CPUSpeed", "Change CPU Clock (unstable)",
              "Overclock the emulated PSP's CPU. 0 = default (222 MHz). Unstable on high values.",
              SettingDef::Int, "0", {}, 0, 1000, 1, "slider", "MHz"});

    // ── Group: Save states ──
    s.append({"System", "", "Save states", "General", "EnableStateUndo", "Savestate slot backups",
              "Keep a one-step undo for save state slots.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"System", "", "Save states", "General", "SaveStateSlotCount", "Savestate slot count",
              "Number of save state slots per game.",
              SettingDef::Int, "5", {}, 1, 30, 1, "slider", ""});
    s.append({"System", "", "Save states", "General", "AutoLoadSaveState", "Auto load savestate",
              "Automatically load a save state when starting a game.",
              SettingDef::Combo, "0",
              {{"Off", "0"}, {"Newest Save", "2"},
               {"Slot 1", "3"}, {"Slot 2", "4"}, {"Slot 3", "5"},
               {"Slot 4", "6"}, {"Slot 5", "7"}}, 0, 0, 0});
    s.append({"System", "", "Save states", "General", "RewindSnapshotInterval", "Rewind Snapshot Interval",
              "Take a rewind snapshot every N seconds. 0 disables rewind.",
              SettingDef::Int, "0", {}, 0, 60, 1, "slider", "s"});

    // ── Group: General ──
    // Restore Default Settings (action), Use system native keyboard (gated
    // SYSPROP_HAS_KEYBOARD), Pause when not focused (force-patched embedding-
    // critical) all omitted.
    s.append({"System", "", "General", "General", "AskForExitConfirmationAfterSeconds", "Ask for exit confirmation after seconds",
              "Show an exit confirmation when the game has been running this long.",
              SettingDef::Int, "300", {}, 0, 1200, 10, "slider", "s"});
    s.append({"System", "", "General", "General", "CacheFullIsoInRam", "Cache full ISO in RAM",
              "Loads the entire ISO into RAM for faster reads. Uses more memory.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"System", "", "General", "General", "CheckForNewVersion", "Check for new versions of PPSSPP",
              "Periodically check ppsspp.org for new releases.",
              SettingDef::Bool, "true", {}, 0, 0, 0});
    s.append({"System", "", "General", "General", "ScreenshotsAsPNG", "Screenshots as PNG",
              "Save screenshots as PNG instead of JPEG.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"System", "", "General", "General", "ScreenshotMode", "Screenshot mode",
              "Whether screenshots include UI overlays.",
              SettingDef::Combo, "0",
              {{"Final processed image", "0"}, {"Raw game image", "1"}}, 0, 0, 0});

    // ── Group: Cheats ──
    s.append({"System", "", "Cheats", "General", "EnableCheats", "Enable Cheats",
              "Apply cheat codes loaded for the running game.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"System", "", "Cheats", "General", "EnablePlugins", "Enable plugins",
              "Load PPSSPP plugins.",
              SettingDef::Bool, "true", {}, 0, 0, 0});

    // ── Group: PSP Settings ──
    // Nickname (sNickName) deferred — text input.
    s.append({"System", "", "PSP Settings", "SystemParam", "GameLanguage", "Game language",
              "Language reported to PSP games.",
              SettingDef::Combo, "-1",
              {{"Auto", "-1"}, {"Japanese", "0"}, {"English", "1"},
               {"French", "2"}, {"Spanish", "3"}, {"German", "4"},
               {"Italian", "5"}, {"Dutch", "6"}, {"Portuguese", "7"},
               {"Russian", "8"}, {"Korean", "9"},
               {"Chinese (traditional)", "10"}, {"Chinese (simplified)", "11"}},
              0, 0, 0});
    s.append({"System", "", "PSP Settings", "SystemParam", "PSPModel", "PSP Model",
              "Reported PSP model.",
              SettingDef::Combo, "1",
              {{"PSP-1000", "0"}, {"PSP-2000/3000", "1"}}, 0, 0, 0});
    s.append({"System", "", "PSP Settings", "SystemParam", "DayLightSavings", "Daylight savings",
              "Apply DST when reporting wall-clock time to games.",
              SettingDef::Bool, "false", {}, 0, 0, 0});
    s.append({"System", "", "PSP Settings", "SystemParam", "ParamDateFormat", "Date Format",
              "PSP-side date format.",
              SettingDef::Combo, "0",
              {{"YYYYMMDD", "0"}, {"MMDDYYYY", "1"}, {"DDMMYYYY", "2"}}, 0, 0, 0});
    s.append({"System", "", "PSP Settings", "SystemParam", "ParamTimeFormat", "Time Format",
              "PSP-side time format.",
              SettingDef::Combo, "0",
              {{"24HR", "0"}, {"12HR", "1"}}, 0, 0, 0});
    s.append({"System", "", "PSP Settings", "SystemParam", "ButtonPreference", "Confirmation Button",
              "PSP-side confirmation button (X for international, O for Japanese region).",
              SettingDef::Combo, "1",
              {{"Use O to confirm", "0"}, {"Use X to confirm", "1"}}, 0, 0, 0});

    return s;
}

// ============================================================================
// ensureConfig — create or patch ppsspp.ini + controls.ini before launch
// ============================================================================

bool PPSSPPAdapter::ensureConfig(const EmulatorManifest& /*manifest*/,
                                 const QString& biosPath,
                                 const QString& savesPath) {
    // Set portable memstick directory so PPSSPP reads config from our managed location.
    // On macOS, PPSSPP checks NSUserDefaults "UserPreferredMemoryStickDirectoryPath" first.
    //
    // Surface a failure of the `defaults` invocation: if PPSSPP ever
    // rebrands its bundle id, sandboxes, or the binary is missing on a
    // hardened OS install, this write goes nowhere and PPSSPP launches
    // against the user's last memstick path. Without an exit-code check
    // that mode of failure is silent — the user just sees their saves
    // disappear.
#if defined(Q_OS_MACOS)
    const int rc = QProcess::execute(
        "defaults",
        {"write", "org.ppsspp.ppsspp",
         "UserPreferredMemoryStickDirectoryPath", configDir()});
    if (rc != 0) {
        qWarning() << "[PPSSPP] `defaults write` failed (exit code" << rc
                   << "); PPSSPP may not pick up the managed memstick path."
                   << "If saves don't appear under" << configDir()
                   << "verify that org.ppsspp.ppsspp is still PPSSPP's bundle id.";
    }
#endif

    // Ensure PSP/SYSTEM directory exists (where PPSSPP reads ppsspp.ini + controls.ini)
    const QString nativeDir = nativeConfigDir();
    if (!QDir().mkpath(nativeDir)) {
        qWarning() << "[PPSSPP] Failed to create PSP/SYSTEM directory:" << nativeDir;
        return false;
    }

    // Our managed config (for UI system — controller type, binding storage)
    const QString mainPath = iniPath();
    const bool ok = QFileInfo::exists(mainPath)
        ? patchExistingConfig(mainPath, biosPath, savesPath)
        : createDefaultConfig(mainPath, biosPath, savesPath);
    if (!ok)
        return false;

    // Remove any malformed hotkey entries in controls.ini so a previously
    // corrupted binding (e.g. "Save State = Keyboard/d") doesn't crash PPSSPP
    // on next launch.
    scrubControlsIniHotkeys();

    return true;
}

// ============================================================================
// resolveExecutable — platform-aware executable resolution
// ============================================================================

QString PPSSPPAdapter::resolveExecutable(const EmulatorManifest& manifest,
                                          const QString& installPath) {
    return resolveExecutableInDir(manifest, installPath, "PPSSPPSDL");
}

// ============================================================================
// createDefaultConfig — write only embedding-critical keys to ppsspp.ini
// ============================================================================

bool PPSSPPAdapter::createDefaultConfig(const QString& path,
                                        const QString& /*biosPath*/,
                                        const QString& /*savesPath*/) {
    // PPSSPP hardcodes every PSP subdirectory (SAVEDATA, PPSSPP_STATE,
    // SCREENSHOT, TEXTURES, Cheats, GAME, PLUGINS, SYSTEM) as literal
    // children of {memstick}/PSP/ — see Core/Util/PathUtil.cpp upstream.
    // There is no INI key to relocate any of them individually, so we only
    // write wizard suppression, fullscreen, and controller type here.
    QStringList lines = {
        "[General]",
        "FirstRun = False",
        "AutoLoadSaveState = 0",
        "EnableStateUndo = True",
        "",
        "[Graphics]",
        "FullScreen = True",
        "",
        "[Sound]",
        "Enable = True",
        "",
        "[Pad1]",
        "Type = Standard",
        "",
    };

    if (!writeConfigFile(path, lines.join("\n"), "PPSSPP"))
        return false;

    // Create default controls.ini with default bindings.
    // "Pause (no menu)" maps to PPSSPP's VIRTKEY_PAUSE_NO_MENU which
    // pauses the game without showing PPSSPP's own UI overlay
    // (perfect for our HUD to live above without conflict). 1-62 =
    // DEVICE_ID_KEYBOARD-NKCODE_SPACE; AppController synthesizes
    // kVK_Space via CGEventPostToPid.
    QStringList ctrlLines = {
        "[ControlMapping]",
        "Up = 10-19",
        "Down = 10-20",
        "Left = 10-21",
        "Right = 10-22",
        "Cross = 10-96,10-189",
        "Circle = 10-97,10-190",
        "Square = 10-99,10-191",
        "Triangle = 10-100,10-188",
        "Start = 10-108,10-197",
        "Select = 10-109,10-196",
        "L = 10-102,10-194",
        "R = 10-103,10-195",
        "An.Up = 10-4003",
        "An.Down = 10-4002",
        "An.Left = 10-4001",
        "An.Right = 10-4000",
        "Fast-forward = 10-4036",
        "Pause (no menu) = 1-62",
        // In-game menu Save/Load State: 1-135 / 1-137 = keyboard F5/F7
        // (DEVICE_ID_KEYBOARD-NKCODE_F5/F7). AppController synthesizes
        // kVK_F5 / kVK_F7. Hidden from hotkeyBindingDefs() so the
        // user can't rebind the synth keys.
        "Save State = 1-135",
        "Load State = 1-137",
        "",
    };

    return writeConfigFile(controlsIniPath(), ctrlLines.join("\n"), "PPSSPP");
}

// ============================================================================
// patchExistingConfig — fix up an existing ppsspp.ini for headless operation
// ============================================================================

bool PPSSPPAdapter::patchExistingConfig(const QString& path,
                                        const QString& /*biosPath*/,
                                        const QString& /*savesPath*/) {
    QString content;
    if (!readConfigFile(path, content, "PPSSPP"))
        return false;

    // See createDefaultConfig() — PPSSPP hardcodes all PSP subdirs under
    // {memstick}/PSP/, so we only patch wizard suppression, fullscreen,
    // and controller type.
    QVector<IniKeyPatch> patches = {
        {"General",  "FirstRun",          "False"},
        {"General",  "AutoLoadSaveState", "0"},
        {"General",  "EnableStateUndo",   "True"},
        {"Graphics", "FullScreen",        "True"},
        {"Pad1",     "Type",              "Standard"},
    };

    if (patchIniKeys(content, patches) && !writeConfigFile(path, content, "PPSSPP"))
        return false;
    return true;
}

void PPSSPPAdapter::scrubControlsIniHotkeys() {
    const QString path = controlsIniPath();
    if (!QFileInfo::exists(path)) return;

    IniFile ini;
    if (!ini.load(path)) {
        qWarning() << "[PPSSPP] Cannot load controls.ini for hotkey scrub:" << path;
        return;
    }

    // PPSSPP input mapping values must match: "<int>-<int>" optionally
    // repeated with ':' (chord) or ',' (alternatives) separators.
    static const QRegularExpression validMapping(
        QStringLiteral("^\\d+-\\d+([:,]\\d+-\\d+)*$"));

    bool changed = false;
    for (const auto& def : hotkeyBindingDefs()) {
        const QString value = ini.value(def.section, def.key);
        if (value.isEmpty()) continue;
        if (!validMapping.match(value).hasMatch()) {
            qWarning().noquote()
                << "[PPSSPP] Removing malformed hotkey" << def.key
                << "=" << value << "from controls.ini";
            ini.setValue(def.section, def.key, "");
            changed = true;
        }
    }

    // Force-inject "Pause (no menu)" / "Save State" / "Load State" so
    // RetroNest's in-game menu can synthesize the bound keys. We
    // always overwrite to defend against the user accidentally
    // clearing them. (The hotkey UI doesn't expose these — see
    // hotkeyBindingDefs.)
    struct ForcedBinding { const char* key; const char* value; };
    static constexpr ForcedBinding kForced[] = {
        {"Pause (no menu)", "1-62"},  // KEYBOARD-NKCODE_SPACE
        {"Save State",      "1-135"}, // KEYBOARD-NKCODE_F5
        {"Load State",      "1-137"}, // KEYBOARD-NKCODE_F7
    };
    for (const auto& fb : kForced) {
        const QString val = QString::fromLatin1(fb.value);
        if (ini.value("ControlMapping", QString::fromLatin1(fb.key)) != val) {
            ini.setValue("ControlMapping", QString::fromLatin1(fb.key), val);
            changed = true;
        }
    }

    if (changed && !ini.save(path))
        qWarning() << "[PPSSPP] Failed to save scrubbed controls.ini:" << path;
}

// ============================================================================
// Paths, BIOS, resolution, aspect ratio
// ============================================================================

QVector<PathDef> PPSSPPAdapter::pathsDefs() const {
    // PPSSPP enforces a fixed directory layout under {memstick}/PSP/ and has
    // no INI keys to relocate individual subdirs. Returning an empty list
    // hides PPSSPP from the Paths Settings screen.
    return {};
}

QVector<BiosDef> PPSSPPAdapter::biosFiles() const {
    // PSP emulation is fully HLE — no required BIOS files
    return {
        {"ppge_atlas.zim", "PSP UI font atlas (optional)", false, ""},
    };
}

ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"720P", "3"}, {"1080P", "4"}, {"1440P", "6"}, {"4K", "8"}},
            "3"};
}

// ============================================================================
// Controller bindings, types, settings
// ============================================================================

QVector<ControllerTypeDef> PPSSPPAdapter::controllerTypes() const {
    return {
        {"Standard", "PSP Controller",
         ":/AppUI/qml/AppUI/images/controllers/PSP.svg"},
    };
}

QVector<BindingDef> PPSSPPAdapter::controllerBindingDefsForType(const QString& type) const {
    if (type != "Standard") return {};

    // PPSSPP controls.ini format: {deviceId}-{keyCode}
    // Device 10 = DEVICE_ID_PAD_0 (generic gamepad)
    // Button keycodes: Android NKCODEs (19=DpadUp, 96=ButtonA, etc.)
    // Axis keycodes: 4000 + (axisId*2) + (negative ? 1 : 0)
    //
    // Spotlight coordinates are in PSP.svg's intrinsic viewBox
    // (2367 × 1014). Centers calibrated against the SVG paths: D-Pad
    // arms around (272, 457) ± 130 px, face buttons around (2108, 457)
    // ± 135, single analog nub at (273, 767), shoulders at (409, 233) /
    // (1960, 234), Select/Start at (1695, 947) / (1850, 948).
    return {
        // D-Pad
        {BindingDef::Button, "Up",       "D-Pad",        "ControlMapping", "Up",       "10-19",
            "DPad", 272, 327, 80},
        {BindingDef::Button, "Down",     "D-Pad",        "ControlMapping", "Down",     "10-20",
            "DPad", 272, 586, 80},
        {BindingDef::Button, "Left",     "D-Pad",        "ControlMapping", "Left",     "10-21",
            "DPad", 143, 456, 80},
        {BindingDef::Button, "Right",    "D-Pad",        "ControlMapping", "Right",    "10-22",
            "DPad", 399, 458, 80},
        // Face Buttons (GameController NKCODE + raw joystick button fallback)
        // Raw buttons: NKCODE_BUTTON_1(188)=Triangle, _2(189)=Cross, _3(190)=Circle, _4(191)=Square
        {BindingDef::Button, "Cross",    "Face Buttons", "ControlMapping", "Cross",    "10-96,10-189",
            "FaceButtons", 2108, 594, 70},
        {BindingDef::Button, "Circle",   "Face Buttons", "ControlMapping", "Circle",   "10-97,10-190",
            "FaceButtons", 2244, 458, 70},
        {BindingDef::Button, "Square",   "Face Buttons", "ControlMapping", "Square",   "10-99,10-191",
            "FaceButtons", 1972, 457, 70},
        {BindingDef::Button, "Triangle", "Face Buttons", "ControlMapping", "Triangle", "10-100,10-188",
            "FaceButtons", 2107, 320, 70},
        // Shoulders — PSP only has L/R (no L2/R2). Raw: _7(194)=L1, _8(195)=R1.
        {BindingDef::Button, "L", "Triggers", "ControlMapping", "L", "10-102,10-194",
            "LeftShoulders", 409, 233, 60},
        {BindingDef::Button, "R", "Triggers", "ControlMapping", "R", "10-103,10-195",
            "RightShoulders", 1960, 234, 60},
        // System (raw: _9(196)=Select, _10(197)=Start)
        {BindingDef::Button, "Start",  "System", "ControlMapping", "Start",  "10-108,10-197",
            "System", 1850, 948, 75},
        {BindingDef::Button, "Select", "System", "ControlMapping", "Select", "10-109,10-196",
            "System", 1695, 947, 75},
        // Analog Stick — PSP has a single nub on the left, so the four
        // axis directions all live in the LeftAnalog cardSlot. Spotlights
        // offset ±60 px from the nub center (273, 767) on each axis.
        // Y-: up (4003), Y+: down (4002), X-: left (4001), X+: right (4000)
        {BindingDef::Axis, "An.Up",    "Analog Stick", "ControlMapping", "An.Up",    "10-4003",
            "LeftAnalog", 273, 707, 70},
        {BindingDef::Axis, "An.Down",  "Analog Stick", "ControlMapping", "An.Down",  "10-4002",
            "LeftAnalog", 273, 827, 70},
        {BindingDef::Axis, "An.Left",  "Analog Stick", "ControlMapping", "An.Left",  "10-4001",
            "LeftAnalog", 213, 767, 70},
        {BindingDef::Axis, "An.Right", "Analog Stick", "ControlMapping", "An.Right", "10-4000",
            "LeftAnalog", 333, 767, 70},
    };
}

QVector<SettingDef> PPSSPPAdapter::controllerSettingDefsForType(const QString& type) const {
    Q_UNUSED(type);
    return {};
}

// ============================================================================
// Hotkeys
// ============================================================================

QVector<HotkeyDef> PPSSPPAdapter::hotkeyBindingDefs() const {
    return {
        // ── Control modifiers ──
        {"Rotate Analog (CW)",       "Control modifiers", "ControlMapping", "Rotate Analog (CW)",  ""},
        {"Rotate Analog (CCW)",      "Control modifiers", "ControlMapping", "Rotate Analog (CCW)", ""},
        {"Analog limiter",           "Control modifiers", "ControlMapping", "Analog limiter",      ""},
        {"RapidFire",                "Control modifiers", "ControlMapping", "RapidFire",           ""},
        {"Axis swap (hold)",         "Control modifiers", "ControlMapping", "Axis swap (hold)",    ""},
        {"Axis swap (toggle)",       "Control modifiers", "ControlMapping", "Axis swap (toggle)",  ""},

        // ── Emulator controls ──
        {"Fast-forward",             "Emulator controls", "ControlMapping", "Fast-forward",                "10-4036"},
        {"SpeedToggle",              "Emulator controls", "ControlMapping", "SpeedToggle",                 ""},
        {"Alt speed 1",              "Emulator controls", "ControlMapping", "Alt speed 1",                 ""},
        {"Alt speed 2",              "Emulator controls", "ControlMapping", "Alt speed 2",                 ""},
        {"Analog speed",             "Emulator controls", "ControlMapping", "Analog speed",                ""},
        {"Reset",                    "Emulator controls", "ControlMapping", "Reset",                       ""},
        {"Frame Advance",            "Emulator controls", "ControlMapping", "Frame Advance",               ""},
        {"Rewind",                   "Emulator controls", "ControlMapping", "Rewind",                      ""},
        // "Save State" and "Load State" are force-bound to keyboard
        // F5/F7 in scrubControlsIniHotkeys and synthesized by
        // AppController for the in-game menu's Save State / Load State
        // actions; omitted here so users can't rebind the synth keys.
        {"Previous Slot",            "Emulator controls", "ControlMapping", "Previous Slot",               ""},
        {"Next Slot",                "Emulator controls", "ControlMapping", "Next Slot",                   ""},
        {"Toggle tilt control",      "Emulator controls", "ControlMapping", "Toggle tilt control",         ""},
        {"Display Portrait",         "Emulator controls", "ControlMapping", "Display Portrait",            ""},
        {"Display Portrait Reversed","Emulator controls", "ControlMapping", "Display Portrait Reversed",   ""},
        {"Display Landscape",        "Emulator controls", "ControlMapping", "Display Landscape",           ""},
        {"Display Landscape Reversed","Emulator controls","ControlMapping", "Display Landscape Reversed",  ""},
        {"Screenshot",               "Emulator controls", "ControlMapping", "Screenshot",                  ""},
        {"Mute toggle",              "Emulator controls", "ControlMapping", "Mute toggle",                 ""},
    };
}

// ============================================================================
// formatBinding — PPSSPP native format
// ============================================================================

QString PPSSPPAdapter::formatBinding(int deviceIndex, const QString& element,
                                      bool isAxis, bool positive) const {
    // PPSSPP controls.ini format: {deviceId}-{keyCode}
    // Device ID: 10 + deviceIndex (DEVICE_ID_PAD_0 = 10)
    // Keycodes: Android NKCODE values for buttons, 4000-based for axes
    const int ppssppDeviceId = 10 + deviceIndex;

    // SDL button names → {GameController NKCODE, raw joystick NKCODE_BUTTON_N fallback}
    // Raw fallback uses NKCODE_BUTTON_1(188)+ series for standard controller layout
    struct ButtonCodes { int gc; int raw; };
    static const QMap<QString, ButtonCodes> buttonToNkcode = {
        {"DPadUp",         {19, -1}},  {"DPadDown",       {20, -1}},
        {"DPadLeft",       {21, -1}},  {"DPadRight",      {22, -1}},
        {"FaceSouth",      {96, 189}}, {"A",              {96, 189}},  // raw b2=189
        {"FaceEast",       {97, 190}}, {"B",              {97, 190}},  // raw b3=190
        {"FaceWest",       {99, 191}}, {"X",              {99, 191}},  // raw b4=191
        {"FaceNorth",     {100, 188}}, {"Y",             {100, 188}},  // raw b1=188
        {"LeftShoulder",  {102, 194}}, {"RightShoulder", {103, 195}},  // raw b7,b8
        {"LeftTrigger",   {104, -1}},  {"RightTrigger",  {105, -1}},
        {"LeftStick",     {106, 199}}, {"RightStick",    {107, 200}},  // raw b12,b13
        {"Start",         {108, 197}}, {"Back",          {109, 196}},  // raw b10,b9
    };

    // SDL axis names → PPSSPP axis IDs (for 4000-based encoding)
    // Formula: 4000 + (axisId * 2) + (negative ? 1 : 0)
    static const QMap<QString, int> axisToId = {
        {"LeftX",   0},    // JOYSTICK_AXIS_X
        {"LeftY",   1},    // JOYSTICK_AXIS_Y
        {"RightX", 11},    // JOYSTICK_AXIS_Z
        {"RightY", 14},    // JOYSTICK_AXIS_RZ
        {"LeftTrigger",  17},  // JOYSTICK_AXIS_LTRIGGER
        {"RightTrigger", 18},  // JOYSTICK_AXIS_RTRIGGER
    };

    if (isAxis) {
        auto it = axisToId.find(element);
        if (it != axisToId.end()) {
            int keyCode = 4000 + (it.value() * 2) + (positive ? 0 : 1);
            return QString("%1-%2").arg(ppssppDeviceId).arg(keyCode);
        }
        // Fallback for unknown axes
        return QString("%1-%2").arg(ppssppDeviceId).arg(element);
    }

    auto it = buttonToNkcode.find(element);
    if (it != buttonToNkcode.end()) {
        QString result = QString("%1-%2").arg(ppssppDeviceId).arg(it->gc);
        // Add raw joystick button fallback (comma-separated alternative)
        if (it->raw >= 0)
            result += QString(",%1-%2").arg(ppssppDeviceId).arg(it->raw);
        return result;
    }
    // Fallback for unknown buttons
    return QString("%1-%2").arg(ppssppDeviceId).arg(element);
}

// ============================================================================
// Keyboard/mouse/wheel bindings — PPSSPP uses DEVICE_ID_KEYBOARD=1 with
// Android NKCODE values. Mouse and wheel captures are not supported for
// hotkeys in this path (PPSSPP mouse bindings use a separate device id and
// we don't wire that into the hotkey capture UI).
// ============================================================================

static int qtKeyToPpssppNkcode(int qtKey) {
    // Letters: NKCODE_A(29) .. NKCODE_Z(54)
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return 29 + (qtKey - Qt::Key_A);
    // Digits: NKCODE_0(7) .. NKCODE_9(16)
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return 7 + (qtKey - Qt::Key_0);
    // Function keys: NKCODE_F1(131) .. NKCODE_F12(142)
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12)
        return 131 + (qtKey - Qt::Key_F1);
    switch (qtKey) {
        case Qt::Key_Return:       return 66;  // NKCODE_ENTER
        case Qt::Key_Enter:        return 66;
        case Qt::Key_Space:        return 62;  // NKCODE_SPACE
        case Qt::Key_Escape:       return 111; // NKCODE_ESCAPE
        case Qt::Key_Tab:          return 61;  // NKCODE_TAB
        case Qt::Key_Backspace:    return 67;  // NKCODE_DEL
        case Qt::Key_Delete:       return 112; // NKCODE_FORWARD_DEL
        case Qt::Key_Up:           return 19;  // NKCODE_DPAD_UP
        case Qt::Key_Down:         return 20;  // NKCODE_DPAD_DOWN
        case Qt::Key_Left:         return 21;  // NKCODE_DPAD_LEFT
        case Qt::Key_Right:        return 22;  // NKCODE_DPAD_RIGHT
        case Qt::Key_Shift:        return 59;  // NKCODE_SHIFT_LEFT
        case Qt::Key_Control:      return 113; // NKCODE_CTRL_LEFT
        case Qt::Key_Alt:          return 57;  // NKCODE_ALT_LEFT
        case Qt::Key_Semicolon:    return 74;  // NKCODE_SEMICOLON
        case Qt::Key_Comma:        return 55;  // NKCODE_COMMA
        case Qt::Key_Period:       return 56;  // NKCODE_PERIOD
        case Qt::Key_Slash:        return 76;  // NKCODE_SLASH
        case Qt::Key_Minus:        return 69;  // NKCODE_MINUS
        case Qt::Key_Equal:        return 70;  // NKCODE_EQUALS
        case Qt::Key_BracketLeft:  return 71;  // NKCODE_LEFT_BRACKET
        case Qt::Key_BracketRight: return 72;  // NKCODE_RIGHT_BRACKET
        case Qt::Key_Backslash:    return 73;  // NKCODE_BACKSLASH
        default: return -1;
    }
}

QString PPSSPPAdapter::formatKeyboardBinding(int qtKey, int modifiers) const {
    // PPSSPP hotkey bindings are single-key; modifier chords aren't supported
    // here, so we ignore modifiers entirely and bind the main key.
    Q_UNUSED(modifiers);
    const int code = qtKeyToPpssppNkcode(qtKey);
    if (code < 0) return {};
    return QString("1-%1").arg(code);
}

QString PPSSPPAdapter::formatMouseBinding(int qtButton) const {
    Q_UNUSED(qtButton);
    return {};
}

QString PPSSPPAdapter::formatWheelBinding(int direction) const {
    Q_UNUSED(direction);
    return {};
}

// ============================================================================
// Serial extraction — PSP uses PARAM.SFO inside ISO
// ============================================================================

QString PPSSPPAdapter::extractSerial(const QString& romPath) const {
    QByteArray sfoData = Iso9660::readFile(romPath, "PSP_GAME/PARAM.SFO");
    if (sfoData.isEmpty()) {
        qWarning() << "[PPSSPP] Failed to read PSP_GAME/PARAM.SFO from:" << romPath;
        return {};
    }
    QString discId = SfoParser::extractDiscId(sfoData);
    if (discId.isEmpty()) {
        qWarning() << "[PPSSPP] No DISC_ID found in PARAM.SFO for:" << romPath;
    }
    return discId;
}

// ============================================================================
// RetroAchievements
// ============================================================================

EmulatorAdapter::RetroAchievementsKeyMap PPSSPPAdapter::retroAchievementsKeyMap() const {
    // PPSSPP uses Title-cased "True"/"False" booleans and prefixes its key
    // names with "Achievements". No notifications key is exposed.
    return {
        "Achievements",                // section
        "AchievementsEnable",          // enabledKey
        "AchievementsHardcoreMode",    // hardcoreKey
        "",                            // notificationsKey (unsupported)
        "AchievementsSoundEffects",    // soundEffectsKey
        "True", "False",               // bool format (Title-case)
        "PPSSPP",                      // configTag
    };
}

// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QVector<EmulatorAdapter::AssetMatchRule> PPSSPPAdapter::assetMatchRules() const {
#if defined(Q_OS_MACOS)
    return { {{"macos"}, ".zip"} };
#elif defined(Q_OS_WIN)
    return { {{"windows", "x64"}, ".zip"} };
#else
    return {
        {{}, ".AppImage"},
        {{"linux"}, ".tar.gz"},
        {{"linux"}, ".tar.xz"},
    };
#endif
}

QVector<SettingsHubCard> PPSSPPAdapter::settingsHubCards() const {
    return {
        // Row 0: Recommended — full-width stretch (across both columns).
        // Same pattern as the Dolphin hub: a curated VIEW of the most-tweaked
        // settings sitting at the top so a user looking for "the dozen
        // settings that actually matter" can find them without hunting
        // through every tab. Same INI keys as the full panes below.
        {QStringLiteral("\U0001F4A1"), "Recommended",
         "Most-tweaked settings — performance, visuals, audio",
         "Recommended", 0, 0, 1, 2},
        // Mirrors PPSSPP's standalone GameSettingsScreen tabs (minus Controls,
        // Tools, VR — see settingsSchema() for rationale).
        // Row 1: Graphics · Audio
        {QStringLiteral("\U0001F5BC"), "Graphics",
         "Backend, resolution, frame pacing, textures, overlays",
         "Graphics", 1, 0},
        {QStringLiteral("\U0001F50A"), "Audio",
         "Backend, latency, volume mix, UI sounds",
         "Audio", 1, 1},
        // Row 2: Networking · System
        {QStringLiteral("\U0001F4F6"), "Networking",
         "Wlan, ad hoc, infrastructure, UPnP, chat",
         "Networking", 2, 0},
        {QStringLiteral("\U0001F527"), "System",
         "Memory stick, save states, cheats, PSP settings",
         "System", 2, 1},
    };
}
