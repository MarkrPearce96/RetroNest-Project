# PCSX2 patches.zip shipment — design

**Date:** 2026-05-18
**Status:** Draft, pre-implementation
**Origin:** [[patches-zip-followup]] memory; surfaced 2026-05-14 during Phase 4 Task 2 live-smoke on DBZ TT2 NTSC.

## Goal

Auto-fetch `patches.zip` from the upstream community repo `github.com/PCSX2/pcsx2_patches` (latest release) into the PCSX2 libretro core's resources directory, so the Phase 4 patches knobs (`pcsx2_enable_widescreen_patches`, `pcsx2_enable_no_interlacing_patches`) and the per-game game-fix DB actually have a data file to act on.

Today these knobs flow correctly to PCSX2, but PCSX2 logs `Failed to open .../patches.zip: No such file` on every game launch and silently skips patch application. User-visible benefit of those knobs is currently zero.

## Background

- The patches DB is maintained in a separate community repo, not bundled in upstream PCSX2 source. Standalone PCSX2 builds download it at build/install time. The libretro core fork did not pick up that pipeline.
- SP7a added `Roboto-Regular.ttf` as a bundled asset under `pcsx2_libretro_resources/` via `dladdr`-based path resolution. `patches.zip` should land in the same directory.
- On-disk destination: `~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/patches.zip`, resolved at runtime by `Pcsx2Libretro::CoreResources::ResolveResourcesDir()` and plumbed to PCSX2 via `EmuFolders::Resources` in `pcsx2-libretro/Settings.cpp:222`.
- RetroNest already has GitHub-Releases fetching infrastructure in `cpp/src/services/emulator_installer.cpp` (async download, SHA256 verification, atomic write, quarantine-strip on macOS). This design reuses the pattern but does not extend that class.

## Non-goals

- **No build-time bundling.** Decoupling app version from patches version is the whole point.
- **No multi-instance lockfile.** RetroNest has no single-instance guard in `main.cpp`, but macOS dock-launch focuses the running instance, so the only path to a double-launch is explicit `open -n` from terminal. Race outcome is at worst a corrupted `.tmp` which the next normal startup re-downloads; not worth the complexity.
- **No generic `ResourcesInstaller` abstraction.** PCSX2 is the only emulator in the RetroNest stable today that uses an externally-released community data bundle. DuckStation cheats, Dolphin Gecko codes, mGBA cheats are all per-file user-drop patterns, not GitHub-released bundles. If a real second consumer ever materializes, refactoring a single-purpose `PatchesInstaller` into a parameterized class is a mechanical job with the actual second use case in hand.
- **No settings-dialog UI showing installed patches version.** Sidecar file exists for the installer's bookkeeping; log line on fetch is sufficient user signal.

## Architecture

### New code

1. **`cpp/src/services/patches_installer.{h,cpp}`** — `PatchesInstaller : public QObject`. Single-purpose, single public entry point.

2. **`cpp/src/services/installer_utils.{h,cpp}`** — small extraction of genuinely shared helpers (`httpGet`, `verifySha256`, atomic-rename). Both `EmulatorInstaller` and `PatchesInstaller` consume them. Mechanical refactor of existing `EmulatorInstaller` private members.

3. **App-startup hook** in `cpp/src/main.cpp` — after wizard accept (or directly on launch if past wizard): run the staleness check, kick off `PatchesInstaller::fetchAsync()` if needed.

4. **Settings-dialog action** in PCSX2 settings page — "Refresh PCSX2 patches" button. Calls the same async path, bypassing the staleness check.

### `PatchesInstaller` public API

```cpp
class PatchesInstaller : public QObject {
    Q_OBJECT
public:
    explicit PatchesInstaller(QObject* parent = nullptr);

    // Resolves resources_dir/patches.zip path, runs staleness check,
    // and returns true if a fetch should run. Cheap; safe to call on
    // the main thread.
    bool isFetchNeeded() const;

    // Kicks off the full state machine on a background thread.
    // Emits progress() during download, finished() on completion.
    // Safe to call when isFetchNeeded() is false — it'll short-circuit
    // and emit finished(true, "already up to date", currentTag).
    void fetchAsync();

signals:
    void progress(qreal fraction, const QString& message);
    void finished(bool success, const QString& message, const QString& tag);
};
```

### Data flow

```
App start (post-wizard)
  │
  ▼
PatchesInstaller::isFetchNeeded()
  ├─ resolve resources_dir via libretro core path or known location
  ├─ check patches.zip exists
  └─ read patches.zip.version sidecar → check installed_at age > 90 days
  │
  ▼ (if needed)
QtConcurrent::run → PatchesInstaller::fetchAsync()
  │
  ├─ GET https://api.github.com/repos/PCSX2/pcsx2_patches/releases/latest
  ├─ parse JSON, locate "patches.zip" asset
  ├─ compare release tag to sidecar tag
  │     │
  │     ├─ same tag + file present + sidecar intact → emit finished(true, "up to date")
  │     └─ different or missing → proceed
  │
  ├─ download to patches.zip.tmp via QNetworkAccessManager
  ├─ SHA256 verify (skip + log if no digest provided by upstream)
  ├─ atomic rename .tmp → patches.zip
  ├─ write patches.zip.version sidecar
  └─ emit finished(true, "updated to <tag>", tag)
  │
  ▼
App main loop → toast on signal → log line
```

### Resolving `resources_dir` from RetroNest

`Pcsx2Libretro::CoreResources::ResolveResourcesDir()` lives in the libretro core and uses `dladdr` from inside the dylib — RetroNest can't call it directly. Instead, RetroNest computes the same path by convention:

```
<libretro install path>/cores/pcsx2_libretro_resources/
```

`<libretro install path>` is already known to RetroNest (it's where `EmulatorInstaller` drops `pcsx2_libretro.dylib`). The `pcsx2_libretro_resources/` subdir is a stable convention shipped by SP7a.

Spec note: if the dylib is missing (PCSX2 not yet installed), `isFetchNeeded()` returns `false` — no point shipping patches without a core to read them.

## Sidecar format

`patches.zip.version`, key=value text alongside the data file. Mirrors `.dylib.version` pattern in `emulator_installer.cpp:389-391` but stores richer metadata for staleness + tag comparison:

```
tag=v2026.04.15
published_at=2026-04-15T14:30:00Z
installed_at=2026-05-18T10:15:42Z
sha256=<hex or empty>
```

Parser tolerates missing keys; absent `installed_at` is treated as "stale, refresh." Absent `tag` is treated as "user-managed file, refresh per staleness rule but warn in log."

## Staleness policy

Fetch runs when **any** of these hold:

- `patches.zip` is missing.
- `patches.zip` exists AND `patches.zip.version` exists AND `(now - installed_at) > 90 days`.
- `patches.zip` exists AND `patches.zip.version` is missing AND `(now - patches.zip mtime) > 90 days`.
- After a fetch is triggered by any rule above, sidecar `tag` matches GitHub's `releases/latest` `tag_name` → short-circuit to "already up to date" without re-downloading.

Critically: `patches.zip` present + sidecar absent does **not** immediately trigger a fetch. That state means the user manually placed the file; respect it until normal staleness kicks in. A simpler rule (just "file missing OR file mtime > 90 days") was considered and rejected: it can't read the upstream tag for the short-circuit step.

## Error handling

| Failure | Behavior |
|---|---|
| Network failure / offline first launch | Log warning, no toast on startup path (don't nag intentional-offline users), retry next app start. Manual refresh button DOES toast on failure since the user explicitly asked. |
| GitHub API rate limit (60/hr unauth) | Treat as network failure. Extremely unlikely at single-user launch frequency. |
| No matching `patches.zip` asset in latest release | Log error, toast failure (suggests upstream renamed the asset), do not touch existing file. |
| SHA256 mismatch (digest present and wrong) | Delete `.tmp`, log error, toast failure, do not touch existing `patches.zip`. |
| Partial download (process killed mid-transfer) | `.tmp` left on disk; next startup overwrites cleanly. No special cleanup. |
| Cmd+Q during fetch | `QCoreApplication::aboutToQuit` aborts the active `QNetworkReply`, mirroring the `9d86291` RAClient pattern. Threadpool joins fast. |
| Existing file without sidecar (user manually placed) | Treat as tag-unknown. Staleness rule still applies; if 90 days have passed, overwrite. Documented behavior — users who want permanent custom files must accept that auto-refresh will replace them. |

## UI surface

- **Startup path:** silent background fetch. On success: toast `"PCSX2 patches updated to <tag>"`. On failure: no toast (suppressed to avoid nagging offline users).
- **Settings dialog:** "Refresh PCSX2 patches" button in the PCSX2 settings page. Calls `fetchAsync()` directly. Both success and failure toast.
- **Log lines:** info-level on success, warning-level on failure. Prefix `[Patches]` to match `[Installer]` style in `emulator_installer.cpp`.

Reuses whatever toast surface `EmulatorInstaller` already drives — TBD which class that is during implementation; spec assumes it's a global `NotificationCenter` or equivalent.

## Testing

### Unit test — `tools/test_patches_installer`

Standalone Qt test binary, mirrors existing `tools/test_core_options` shape. Coverage:

- `isFetchNeeded()` returns true when `patches.zip` missing.
- `isFetchNeeded()` returns true when sidecar missing.
- `isFetchNeeded()` returns true when `installed_at` > 90 days old.
- `isFetchNeeded()` returns false when file present, sidecar fresh.
- `isFetchNeeded()` returns false when libretro dylib not yet installed.
- Sidecar parser tolerates missing keys, malformed lines, empty file.
- Sidecar writer round-trips through the parser.

HTTP-dependent paths (release JSON parsing, asset matching, download) are tested via a small injectable HTTP-get callable — same approach `EmulatorInstaller` would benefit from if it had unit tests.

### Smoke test (manual, on clean machine)

1. Wipe `~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/patches.zip` and `.version`.
2. Launch RetroNest.
3. Confirm log line `[Patches] Fetched v<tag> to <path>`.
4. Confirm `patches.zip` and `patches.zip.version` exist at expected paths.
5. Confirm success toast appears.
6. Launch DBZ TT2 or R&C 2 with `pcsx2_enable_widescreen_patches=enabled`.
7. Confirm visible widescreen effect (game renders 16:9 instead of 4:3 pillarboxed).
8. Re-launch RetroNest; confirm no second fetch (within 90 days), no toast.
9. Manually `touch -t 202401010000 patches.zip.version` to force staleness; re-launch; confirm refetch.
10. Disconnect network; launch RetroNest with file deleted; confirm app starts cleanly, warning logged, no toast, no hang.
11. Cmd+Q during an active fetch; confirm clean shutdown within ~1s.

## Open implementation details (to settle during planning)

- Exact toast/notification class name in current codebase (`NotificationCenter`? `Toast`? Inline?) — verify during plan-writing.
- Whether to add `httpGet` and `verifySha256` to `installer_utils` as free functions or a small `Utils` namespace.
- Whether the staleness threshold (90 days) should be a compile-time constant or settings-tunable. Default to constant; revisit if users complain.

## Time estimate

End-to-end as a self-contained sub-project: **1–2 days.**

- Spec + plan: half-day (covered).
- Implementation (installer + utils + startup hook + settings button + unit test): half to full day.
- Smoke test: short.
- Memory close-out + commit + push: short.

## Hard "don't" list (carried forward from [[patches-zip-followup]])

- Don't bundle `patches.zip` at build time.
- Don't silently overwrite a file modified by the user (the sidecar + tag-comparison logic handles this; if no sidecar, fall back to staleness rule and document the behavior).
- Don't fetch synchronously on the main thread. Use `QtConcurrent::run` paired with `QCoreApplication::aboutToQuit` cancellation per the `9d86291` RAClient pattern.

## Related memories

- [[patches-zip-followup]] — original pickup memory; will be closed when this ships.
- [[project-pcsx2-libretro-port]] — SP7c entry covers the Phase 4 knobs this work makes useful.
- [[bug-c-pickup]] — Cmd+Q race history; the `aboutToQuit` wiring is the prevention pattern.
- [[sp7c-kickoff]] — Phase 4 Task 2 (Display sub-tab) is where the missing file first surfaced.
