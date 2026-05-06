# Libretro in-process backend — design

**Date:** 2026-05-06
**Status:** Draft, pending implementation plan
**First shipped core:** mGBA (covers GBA / GB / GBC)

## Goal

Add an in-process libretro emulation backend to RetroNest. RetroNest loads a libretro core's `.dylib` directly via `dlopen`, owns the video / audio / input pipelines, and renders the core's frames inside the existing Qt main window. Ship `mgba_libretro.dylib` as the first concrete core, covering GBA / GB / GBC. Full RetroAchievements support via a vendored `rcheevos` client running on the core thread.

The architecture is generic: future cores (snes9x, genesis_plus_gx, fceumm, mupen64plus-next, …) are a manifest + dylib drop. No new C++ subsystems per core; only a small per-core adapter subclass for things that have to be code (controller binding curation, RA console-id mapping, settings schema).

## Non-goals (v1)

- No GL / Vulkan / hardware-accelerated cores. Software pixel-format video only (`RGB565`, `XRGB8888`, `0RGB1555`).
- No netplay (libretro's `RETRO_ENVIRONMENT_SET_NETPLAY_*` is unimplemented).
- No multi-instance / multi-core simultaneous sessions.
- No per-game core option overrides. Options are per-core globally.
- No fast-forward, no rewind, no slot-state UI surface in v1. State save/load via the existing save-and-quit / resume flow only.
- No subprocess sandboxing of the core. A core segfault crashes RetroNest.
- No second window. The core renders inside the existing Qt window via a `QQuickItem`, consistent with the project's "one window, one UI" rule.

## Decisions and rationale

| Decision | Choice | Rationale |
|---|---|---|
| **Q1: Scope** | Generic libretro substrate, mGBA as first core | Marginal MVP cost over mGBA-only; pays off the next time we add a core. The user-visible surface is still "ship mGBA." |
| **Q2: Adapter shape** | `LibretroAdapter` base class + thin per-core subclass | Mirrors the rest of `cpp/src/adapters/`. Per-core C++ is unavoidable for things data can't express (controller binding tables, RA console-id, formatBinding overrides). |
| **Q3: Threading & window** | Worker thread runs `retro_run()`; frames blitted into a `QQuickItem` in the existing Qt window | Isolates emulation from UI hitches. Triple-buffer + queued frame-ready signal. Pause = stop pumping. GL-HW path slots into the same `QQuickItem` later via `QSGTexture`. |
| **Q4: Settings storage** | JSON sidecar at `{root}/emulators/libretro/{coreId}/options.json`; `SettingDef::Storage::LibretroOption` discriminator | Aligns with libretro's typed-option model. Smallest possible extension to the existing schema pipeline (one new enum field, one dispatch site). Hand-curated per-core schemas continue to mirror upstream UI verbatim. |
| **Q5: Core distribution** | Buildbot fetch at install time; `disable-library-validation` entitlement; quarantine bit stripped on install | Same path other libretro frontends take on macOS. Manifest gains `backend` / `core_dylib` / `core_buildbot_path`; existing `EmulatorInstaller` flow handles the rest. Build-from-source is the architecturally cleanest answer but doesn't scale across N cores in CI; vendored binaries don't generalize past v1. |
| **Q6: RetroAchievements** | Vendor `rcheevos`; new `RcheevosRuntime` module owns detection; existing `RAService` continues to own UI surfaces and login | Single vendored C library, ~200 KB binary cost. Existing toast / popup / first-launch-login UX unchanged. Hardcore mode enforced by rcheevos itself. |

## Component map

```
cpp/src/adapters/libretro/
  libretro_adapter.{h,cpp}        — base, EmulatorAdapter subclass; orchestrates the runtime
  mgba_libretro_adapter.{h,cpp}   — first concrete subclass; settingsSchema, RA console-id
                                     mapping, controller binding curation, system mappings

cpp/src/core/libretro/
  core_loader.{h,cpp}             — dlopen + retro_* symbol table; one dylib at a time
  core_runtime.{h,cpp}             — owns the core thread; pause/resume/shutdown lifecycle
  environment_callbacks.{h,cpp}   — retro_environment_t dispatch (the v1 enum subset)
  video_software.{h,cpp}            — RGB565 / XRGB8888 / 0RGB1555 software path; double-buffered QImage pool
  audio_sink.{h,cpp}              — SDL2 SDL_AudioStream wrapper; sample-rate conversion
  input_router.{h,cpp}            — RetroPad device model; reads from SdlInputManager
  options_store.{h,cpp}           — JSON-backed core options; hot-reload signaling
  rcheevos_runtime.{h,cpp}        — in-process RA: login, memory map, frame tick, unlock signals
  retro_log.{h,cpp}                — RETRO_ENVIRONMENT_GET_LOG_INTERFACE → qInfo trampoline

cpp/src/ui/libretro/
  libretro_video_item.{h,cpp}     — QQuickItem; receives QImage frames, blits to scene

qml/AppUI/
  EmulationView.qml                — fullscreen QML page hosting the LibretroVideoItem;
                                      overlays the existing in-game menu

manifests/mgba.json                — alongside the others; new fields: backend, core_dylib,
                                      core_buildbot_path

vendor/libretro-api/libretro.h    — single-file header from libretro/RetroArch repo, vendored
```

## Subsystem responsibility lines

- **`LibretroAdapter` (base)** is everything an `EmulatorAdapter` is, plus a `CoreRuntime*` field. `start()` / `stop()` drive the runtime, not a `QProcess`. Most existing methods (`resolveExecutable`, `additionalLaunchArgs`, etc.) are overridden to fit the libretro shape (e.g. `resolveExecutable()` returns the dylib path so the existing `isInstalled()` works unchanged).
- **`MgbaLibretroAdapter`** is small: hand-curated `settingsSchema()` mirroring RetroArch's mGBA core-options menu, `controllerTypes()` declaring "GBA" with the correct GBA-button labels, `biosFiles()` declaring the optional `gba_bios.bin`, RA console-id resolution per-system (the same dylib runs three RA consoles depending on the launching `systemId`).
- **`CoreRuntime`** is the lifecycle owner. One `QThread` per session. State machine: `Idle → Loading → Running ↔ Paused → Stopping → Idle`. All other libretro modules are owned by it as members, destroyed when the thread joins.
- **`EnvironmentCallbacks`** is the bridge libretro cores call into. The C `retro_environment_t` callback trampolines through a per-core thread-local `CoreRuntime*`. v1 implements only the enums mGBA actually uses: log interface, system directory, save directory, pixel format, geometry info, core options v2, variable, variable-update, memory map, performance interface, input descriptors. Anything else returns `false` and logs the unknown enum once per session.
- **`VideoSoftware`** receives `(framebuffer, width, height, pitch)` from the core's `retro_video_refresh_t`, copies into a double-buffered `QImage` pool sized to `max_w × max_h`, and queues a `frameReady(QImage)` signal to the QML thread. Supports `RETRO_PIXEL_FORMAT_RGB565`, `XRGB8888`, `0RGB1555` (the formats mGBA emits, plus the universal default). The class is sized to be replaced/extended with a `VideoHardware` later without touching anything outside `cpp/src/core/libretro/`.
- **`AudioSink`** wraps an `SDL_AudioStream` opened at the device's native sample rate. `retro_audio_sample_batch_t` writes into the stream from the core thread; SDL2's audio thread pulls. Built-in resampling handles mGBA's 32768 Hz → device's 48000 Hz mismatch. Output buffer sized for ~50 ms latency.
- **`InputRouter`** owns one `std::array<std::atomic<uint16_t>, NUM_PORTS>` of RetroPad button bitmasks plus analog axis atomics. `SdlInputManager` is extended with an "emulation mode" callback that writes into these on the Qt main thread; the core thread reads them in `retro_input_state_t`. Existing Cmd+Esc / Select+Circle in-game-menu detection is unchanged. Mappings (physical SDL button → RetroPad slot) come from per-core `controllerBindingDefs()` translated into a flat `(deviceIdx, sdlElement) → RetroPadSlot` lookup table at session start.
- **`OptionsStore`** is the JSON sidecar. Loads from `{root}/emulators/libretro/{coreId}/options.json` at session start; if missing, materialized from the core's `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2` payload using each option's default. **On every load, the store reconciles against the core's current option list:** option keys the core no longer recognizes are dropped (warned), option keys the core newly added are appended with their defaults. This handles core-version upgrades without losing user values for keys that survived. Settings UI writes go through this store. Sets a `dirty` flag the environment callback sees on the next `RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE` query.
- **`RcheevosRuntime`** depends on `CoreLoader` (for `retro_get_memory_data` / `retro_memory_map` access), the existing `RACredentials` for token, and the existing `RAService` for UI signals. Vendored `rcheevos` source pinned via `FetchContent` in CMake. Frame-tick called from `CoreRuntime` after each successful `retro_run()`. Unlocks signaled via `QMetaObject::invokeMethod` to `RAService` so the existing toast UI fires automatically.

## Manifest schema extension

Two new optional fields:

```json
{
  "backend": "libretro",
  "core_dylib": "mgba_libretro.dylib",
  "core_buildbot_path": "mgba_libretro.dylib.zip"
}
```

- `backend` defaults to `"process"` when absent. Existing manifests are untouched.
- `core_dylib` is the filename of the libretro core's `.dylib` after install.
- `core_buildbot_path` is the path component appended to the buildbot URL prefix (`https://buildbot.libretro.com/nightly/apple/osx/<arch>/latest/`). Different from `core_dylib` because the buildbot ships zipped.

`ManifestLoader` learns the new fields. `EmulatorInstaller` branches on `backend == "libretro"` to use buildbot-fetch instead of GitHub Releases.

## Settings extension

`SettingDef` gains one field:

```cpp
enum class Storage { Ini, LibretroOption };
Storage storage = Storage::Ini;
```

Default is `Ini` — no existing schema entry changes. Libretro `SettingDef`s set `Storage::LibretroOption` and use `key` as the libretro option key (e.g. `"mgba_skip_bios"`). `GenericSettingsPage` gains one dispatch site at load (`switch (def.storage)`) and one at save:

```cpp
QString readValue(const SettingDef& def) {
    switch (def.storage) {
        case Storage::Ini:            return readIniValue(def);          // existing
        case Storage::LibretroOption: return optionsStore->get(def.key); // new
    }
}
void writeValue(const SettingDef& def, const QString& v) {
    switch (def.storage) {
        case Storage::Ini:            writeIniValue(def, v); break;
        case Storage::LibretroOption: optionsStore->set(def.key, v); break;
    }
}
```

The `OptionsStore` instance lives on the adapter (created lazily, persists across sessions). The dispatch is shallow because all the layout/widget infrastructure is storage-agnostic — only the read/write pair changes. Edits while a game is running set the `optionsUpdated` flag so the next `RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE` query returns true and the core re-fetches.

## Folder layout (runtime)

Cores are shared, per-system data is per-system. Mirrors the existing rule:

```
{root}/emulators/libretro/
  cores/
    mgba_libretro.dylib                — installed once, used by gba/gb/gbc
    mgba_libretro.dylib.version        — buildbot Last-Modified for update detection
  mgba/                                — per-coreId, NOT per-system, for things shared across the core's systems
    options.json                       — JSON sidecar for libretro core options
    controls.json                      — per-controller-type RetroPad bindings
  gba/                                 — per-system runtime data
    savestates/{romBaseName}.state{N}
    savestates/{romBaseName}.resume    — auto-save-on-exit slot
    saves/{romBaseName}.srm
    screenshots/, cache/
  gb/   ...same shape as gba/
  gbc/  ...same shape as gba/
```

`{root}/bios/` continues to be the shared BIOS root. `LibretroAdapter` overrides `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` to return that path; the core looks for `gba_bios.bin` there. `MgbaLibretroAdapter::biosFiles()` declares it as optional (mGBA runs fine on the open-source bootrom).

`isInstalled()` for libretro adapters checks for `cores/{core_dylib}` rather than a `.app` bundle. `EmulatorAdapter::resolveExecutable()` returns that path so the existing infrastructure works unchanged.

## Threading & session lifecycle

`GameService::startGame(romPath, "mgba", ...)` flow:

1. `GameService` calls `m_session.start(...)` exactly like today. **`GameSession` becomes backend-aware via internal branching**: a private `m_backend` discriminator set from `manifest.backend` selects between the existing `QProcess` path and a new `CoreRuntime` path. Same `start` / `started` / `finished` / `errorOccurred` signals; same `kill` / `terminate` semantics; no public API change. This is the smallest possible blast radius — `GameService`, `AppController`, every QML caller, save-and-quit, resume-state — keep working. (We deliberately do not split `GameSession` into `ProcessGameSession` / `LibretroGameSession` subclasses for v1: the two paths share enough state — pending-save tracking, current ROM path, terminate-then-kill escalation — that subclassing would force most of `GameSession` into a base class anyway.)
2. `LibretroAdapter::ensureConfig()` prepares the per-system data dirs and the `options.json` (creating it from defaults if missing).
3. `CoreRuntime::start(adapter, manifest, romPath)`:
   - `CoreLoader::open(corePath)` → `dlopen` the dylib, resolve all `retro_*` symbols, fail loudly if any required symbol is missing.
   - `retro_set_environment(envCb)` → install our environment callback. Cores call this immediately, before anything else, to declare their core options and capabilities.
   - `OptionsStore::load(systemDir, coreOptions)` → load JSON, create-from-defaults if missing.
   - `retro_init()`.
   - `retro_load_game({ data, size, path })` → for v1 we always pass `path` and let the core do its own file IO (mGBA prefers this; saves us mmap'ing 32 MB ROMs).
   - `retro_get_system_av_info(...)` → `geom { base_w, base_h, max_w, max_h, aspect }` and `timing { fps, sample_rate }`. Hand `geom` to `VideoSoftware` to size its buffers; hand `sample_rate` and `fps` to `AudioSink` and the frame pacer.
   - `RcheevosRuntime::beginSession(systemId, romPath)` → log in if needed (emit `raEmulatorLoginPrompt` on the main thread first if no token), build the rcheevos memory descriptor list from `retro_memory_map` / `retro_get_memory_data` (synchronous, in-process, no network), then **kick off the achievement-set fetch from RA's web API asynchronously** so the start path doesn't block on the network. The first few `retro_run` frames may run before the achievement set is loaded; that's harmless — the rcheevos client just has nothing to evaluate until the set arrives.
   - Spin up the core thread; start polling `retro_run()`.
4. **Core thread main loop** (pseudocode):
   ```
   while (!stopRequested) {
       waitWhilePaused();
       inputRouter.snapshot();        // copy atomic state to a frame-stable snapshot
       retro_run();                   // calls our video/audio/input callbacks
       rcheevosRuntime.frame();       // tick achievement detection
       framePacer.sleepUntilNextFrame();
   }
   ```
5. **Pause/resume.** In-game menu opens → `CoreRuntime::pause()` flips a `std::atomic<bool>` and signals a `condition_variable`. Core thread blocks until `resume()`. Audio sink stops pulling (silence, no popping). Video item shows the last frame frozen.
6. **Save-and-quit.** `GameSession::terminate()` → `CoreRuntime::saveAndStop()`: pause core, `retro_serialize` to `{savestatesDir}/{romBaseName}.resume`, then signal stop. Same UX as PCSX2's SaveStateOnShutdown.
7. **Shutdown.** `CoreRuntime::stop()` → set stop flag, signal core thread, join (5-second timeout, then detach + log). `retro_unload_game`, `retro_deinit`, `dlclose`. `RcheevosRuntime` flushes any pending unlock posts.
8. `GameSession` emits `finished(0, false)` → existing `GameService` cleanup runs unchanged.

## Resume detection

- `MgbaLibretroAdapter::extractSerial(romPath)` reads the system-specific cartridge header. GBA: `0xA0..0xAB` for the 12-byte game code. GB / GBC: `0x0134..0x0143` for the 16-byte title (terminated by `0x00` or CGB flag). System is determined by file extension since one mGBA dylib backs three systems. Stored in the DB at import time, same as today. Future libretro adapters override `extractSerial` with their own header logic; the base class returns empty string by default.
- `LibretroAdapter::findResumeFile(serial)` scans `{root}/emulators/libretro/{systemId}/savestates/` for a `.resume` file whose name matches the ROM with that serial. The base class implementation suffices — no per-core override needed.
- `resumeLaunchArgs()` is unused — there's no CLI. The adapter exposes a `resumeStatePath()` instead. `CoreRuntime::start` reads it after `retro_load_game` and calls `retro_unserialize`. `GameSession` change: pass through the resume path to the runtime instead of via CLI.

## Controllers

Libretro defines a fixed virtual controller called the RetroPad. v1 supports just the standard digital RetroPad — 14 buttons (B / Y / Select / Start / Up / Down / Left / Right / A / X / L / R / L2 / R2) plus 2 analog sticks. mGBA's GBA mapping uses the digital subset only.

- `MgbaLibretroAdapter::controllerTypes()` returns one entry: `"GBA"` (label on the mapping page becomes "GBA" with proper button labels — A, B, L, R, Start, Select, D-pad).
- `controllerBindingDefs()` returns the GBA-relevant RetroPad slots only; D-pad, A, B, L, R, Start, Select. Hidden slots (X / Y / L2 / R2 / L3 / R3 / analog sticks) are not surfaced for mGBA but the framework can render them for future cores.
- Bindings persist to `{root}/emulators/libretro/{coreId}/controls.json` (libretro has no analog INI; we own this storage).
- `formatBinding()` writes the canonical SDL element name. `InputRouter` reads the JSON at session start, builds an `array<RetroPadSlot, 16>` lookup keyed on `(deviceIdx, sdlElement)`. No INI patching, no per-emulator format string weirdness.

## Hotkeys

In-game menu hotkeys (Cmd+Esc, Select+Circle) are already global, owned by `SdlInputManager` / Carbon. They keep working; nothing libretro-specific needed.

`hotkeyBindingDefs()` for v1 returns just the in-game-menu hotkey. Future: save-state slot select, fast-forward toggle, rewind. Out of scope for v1.

## macOS install flow

1. `LibretroAdapter::resolveDirectDownload(manifest)`:
   - Resolve buildbot URL: `https://buildbot.libretro.com/nightly/apple/osx/<arch>/latest/<core_buildbot_path>` where `<arch>` is `arm64` or `x86_64` per `QSysInfo::currentCpuArchitecture()`.
   - HEAD the URL to get `Last-Modified`. Pack into `DirectDownloadInfo` with the `Last-Modified` value used both as `publishedAt` (ISO 8601) and as a synthetic `version` string (e.g. `"nightly-2026-05-06"`). The buildbot has no semantic version for cores, so the date IS the version surfaced in the UI.
2. `EmulatorInstaller`:
   - Downloads the zip.
   - Unzips to `{root}/emulators/libretro/cores/`.
   - Runs `xattr -d com.apple.quarantine` on the dylib (silently ignore exit code — file may not have the attr).
   - Writes `{root}/emulators/libretro/cores/{core_dylib}.version` with the buildbot's `Last-Modified` timestamp.
3. `RetroNest.entitlements` gains:
   ```xml
   <key>com.apple.security.cs.disable-library-validation</key>
   <true/>
   ```
   The build system applies the entitlement at codesign time; documented in the entitlements file as required for libretro core loading.
4. Update detection: `EmulatorInstaller` periodic check compares stored `version` against current via HEAD. Same surface as today's "Update available" flow.

The setup wizard surface is unchanged. Because `manifests/mgba.json` lives alongside `manifests/dolphin.json`, the wizard sees mGBA through `ManifestLoader` exactly like every other emulator: it appears on the emulator-selection, resolution, aspect-ratio, and BIOS pages with no wizard code changes.

## Settings UI integration

`MgbaLibretroAdapter::settingsSchema()` returns `SettingDef`s with `Storage::LibretroOption`. The schema is hand-curated to mirror RetroArch's "Quick Menu → Core Options → mGBA" layout: same group order, same labels, same gating chains where applicable. Mirroring upstream is the project convention (per the alignment memory entries) and stays intact.

The Recommended page hosts an `AspectRatioPreview` widget keyed on the `mgba_aspect_ratio` core option, reusing the existing widget unchanged.

`patchRetroAchievements()` is unused for libretro — there's no INI to patch. `LibretroAdapter` overrides it to push the four prefs (enabled / hardcore / notifications / sounds) into the live `RcheevosRuntime` config, with no-op when no game is running. The settings UI doesn't need to know.

## RetroAchievements wiring

- New CMake `FetchContent_Declare(rcheevos)` pinned to a release tag. Built as a static lib, linked into RetroNest.
- `RcheevosRuntime` opens an rcheevos client at session start, hands it `RACredentials::token()` if present, otherwise emits the existing `raEmulatorLoginPrompt` signal and blocks the core thread until login completes (or session is cancelled).
- After `retro_load_game`, the runtime queries the core's memory regions via `retro_memory_map` (preferred, modern cores) or per-region `retro_get_memory_data(RETRO_MEMORY_*)` (older cores). mGBA supports `retro_memory_map`. Build the rcheevos memory descriptor list, register with the rcheevos client.
- Per-frame: `rc_client_do_frame(client)` after each `retro_run()`.
- Unlock callbacks → emit Qt signals queued to the main thread → `RAService` shows the existing achievement toast.
- Hardcore mode: read from existing `RAService` setting at session start; rcheevos enforces "no save-state load" automatically.
- `LibretroAdapter::supportsRetroAchievements()` → `true`.
- `MgbaLibretroAdapter::raConsoleId(systemId)` returns the right RA console ID per system: GBA=5, GB=4, GBC=6.

`ra_client.cpp::consoleIdMapping()` already includes `gba`, `gb`, `gbc` mappings for the standalone-emulator pre-cache flow; verify and extend if any are missing.

## Stated tradeoffs / deferrals

Each is an accepted v1 limit, with the rationale captured for future-us:

- **No crash isolation.** A core segfault crashes RetroNest. *Why:* mGBA is rock-solid in practice; subprocess sandboxing reintroduces IPC and breaks the in-process rcheevos plan; signal-handler-based recovery in C++ is fragile (signal handlers can't safely re-enter Qt). *How to apply:* acceptable for mGBA-class cores. Reassess if/when we adopt cores with a documented crash history (ParaLLEl-N64, certain Mednafen builds).
- **Software video only.** No GL / Vulkan HW context. *Why:* mGBA is software-rendered. The `VideoSoftware` module's seam is sized to be replaced by a `VideoHardware` later without touching `CoreRuntime` / `LibretroAdapter` / anything outside `cpp/src/core/libretro/`. *How to apply:* when we add a HW-only core, bring up the GL path. Refusing to load HW cores at install time is fine until then.
- **Single core thread / no multi-instance / no netplay.** *Why:* v1 doesn't need it. Netplay (libretro's `RETRO_ENVIRONMENT_SET_NETPLAY_*`) is its own subsystem.
- **No per-game core option overrides.** *Why:* matches today's UX; needs a UI surface that doesn't exist yet. *How to apply:* add `options.{romBaseName}.json` overlay to `OptionsStore` when we want it.
- **No fast-forward, no rewind, no slot-state UI in v1.** *Why:* scope. *How to apply:* straightforward `CoreRuntime` extensions; design doesn't preclude.
- **`disable-library-validation` entitlement is a real security loosening.** *Why:* required to load any unsigned dylib in a hardened-runtime app. *How to apply:* if we ever notarize for App Store, this needs to be replaced with notarized cores or core re-signing post-download.
- **Setup wizard's BIOS page treats mGBA's `gba_bios.bin` as optional.** Most users won't have one; mGBA's open-source bootrom is fine. The wizard already handles "BIOS missing → warn but allow continue" for optional BIOS files.

## Files touched / created

**New:**
- `cpp/src/adapters/libretro/{libretro_adapter, mgba_libretro_adapter}.{h,cpp}`
- `cpp/src/core/libretro/{core_loader, core_runtime, environment_callbacks, video_software, audio_sink, input_router, options_store, rcheevos_runtime, retro_log}.{h,cpp}`
- `cpp/src/ui/libretro/libretro_video_item.{h,cpp}`
- `qml/AppUI/EmulationView.qml`
- `manifests/mgba.json`
- `vendor/libretro-api/libretro.h`
- `tests/libretro/` (unit tests + fake-core fixture; see Testing plan)

**Modified (small):**
- `cpp/src/core/setting_def.h` — add `Storage` enum + field.
- `cpp/src/core/manifest.h` / `manifest_loader.cpp` — add `backend` / `core_dylib` / `core_buildbot_path` fields.
- `cpp/src/core/game_session.{h,cpp}` — branch on `manifest.backend`; preserve all existing `QProcess` paths.
- `cpp/src/services/emulator_installer.cpp` — branch on `backend == "libretro"` for buildbot fetch + quarantine strip.
- `cpp/src/ui/settings/generic_settings_page.cpp` — `Storage` dispatch in read/write.
- `cpp/src/core/sdl_input_manager.{h,cpp}` — emulation-mode shared state for `InputRouter`.
- `cpp/src/services/ra_service.{h,cpp}` — public `notifyAchievementUnlocked` slot connected by `RcheevosRuntime`.
- `cpp/src/adapters/adapter_registry.cpp` — register `MgbaLibretroAdapter` for `"mgba"`.
- `cpp/src/ui/theme_context.cpp`, `cpp/src/core/scraper.cpp`, `cpp/src/core/ra_client.cpp` — verify gba / gb / gbc mappings exist; add any missing.
- `cpp/CMakeLists.txt` — `FetchContent` for rcheevos; SOURCES additions; entitlement update on macOS.
- `RetroNest.entitlements` — `disable-library-validation`.

## Testing plan

- **Unit:** `OptionsStore` (load / create-from-defaults / round-trip / dirty-flag). `InputRouter` (binding lookup). `EnvironmentCallbacks` (the v1 enum subset). `CoreLoader` (symbol resolution failure modes — missing required symbol fails loudly; missing optional symbol is tolerated).
- **Integration with a fake core:** ship a tiny test "core" `.dylib` in `tests/fixtures/` that implements the libretro ABI but emits a known checkerboard pattern and predictable audio. Drives the full `CoreRuntime` lifecycle without needing mGBA. Lets us regress threading + lifecycle independently of any real core.
- **Smoke test with real mGBA:** load a public-domain GBA homebrew (e.g. one of the Tonc demos) and assert the core boots, runs >60 frames without segfaulting, audio is non-silent, and a `retro_serialize` / `retro_unserialize` round-trips correctly.
- **RA smoke:** mock the rcheevos network layer; verify memory-map registration and frame ticking happen without errors. Real RA login is a manual test step.
- **Manual QA checklist:** install via wizard, launch GBA homebrew, confirm video / audio / input, open in-game menu (pause), resume, save-and-quit, relaunch and resume, settings round-trip (toggle a core option, see it persist).

## Open questions / future work

- HW (GL) video path — bring up when we add the first HW-only core. The architecture seam is documented above.
- Per-game core option overrides — `options.{romBaseName}.json` overlay.
- Fast-forward / rewind / save-state slot UI.
- Subprocess sandboxing of cores known to crash. Likely worth doing if we adopt mupen64plus-next or similar.
- App Store notarization story for the `disable-library-validation` entitlement — only relevant if we ever pursue Mac App Store distribution.
