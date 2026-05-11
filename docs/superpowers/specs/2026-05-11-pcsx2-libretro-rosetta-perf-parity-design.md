# SP10 — PCSX2 libretro Rosetta perf parity (universal-binary approach)

**Status:** Spec — awaiting user review before plan
**Date:** 2026-05-11
**Sub-project:** 10 of the PCSX2 libretro port (see `pcsx2_libretro_port.md`)

## Problem

After SP5 shipped, R&C 2 in RetroNest's libretro PCSX2 core measures ~65–70%
emulation speed on Apple M4. The same scene runs at full speed in standalone
PCSX2 v2.6.3.app on the same hardware.

Root cause is established and not a regression in our shim: upstream PCSX2
has no AArch64 recompilers landed yet. `VMManager.cpp:2720-2732` dispatches
to `intCpu` / `psxInt` / `CpuIntVU0` / `CpuIntVU1` whenever `_M_X86` is not
defined; `pcsx2/arm64/RecStubs.cpp` is placeholder stubs. Any native-arm64
PCSX2 — ours or hypothetical — runs the EE / IOP / VU on interpreters and
hits the same ceiling.

The standalone PCSX2 v2.6.3.app the user compares against is a non-fat
**x86_64-only** binary (`file` confirms). It runs through Rosetta 2 and
beats the native arm64 interpreters because Rosetta translates the x86
recompiler-generated code to arm64 at a fraction of the cost of
running interpreters directly.

## Goal

Get our libretro PCSX2 core to standalone-equivalent speed by making it
loadable in an x86_64-via-Rosetta RetroNest process. Specifically: ship a
universal `RetroNest.app` plus universal libretro cores, so the user can
flip into Rosetta mode via Finder → Get Info → "Open using Rosetta",
relaunch, and have `pcsx2_libretro.dylib`'s x86_64 slice loaded in-process
with full recompiler support.

## Success criteria

1. `file build-universal/RetroNest.app/Contents/MacOS/RetroNest` →
   `Mach-O universal binary with 2 architectures: [x86_64] [arm64]`.
2. Same `file` output for `pcsx2_libretro.dylib` and `mgba_libretro.dylib`
   in `~/Documents/RetroNest/emulators/libretro/cores/`.
3. With "Open using Rosetta" checked: process is x86_64, R&C 2 same-scene
   speed within ±2% of standalone PCSX2 v2.6.3.app's measured speed.
4. Without Rosetta: process is arm64, mGBA full speed (no regression),
   PCSX2 runs at the existing ~65–70% (no regression).
5. Three-way comparison documented in the SP10 results note (native arm64
   libretro vs. Rosetta x86_64 libretro vs. standalone reference).

## Switch-model decision

**Universal `.app` + macOS Get-Info toggle.** RetroNest is built as a
universal binary and ships one bundle. The user controls arch via the
Finder Get-Info "Open using Rosetta" checkbox, which flips the next
launch into x86_64-via-Rosetta. RetroNest itself does no auto-relaunching
or arch-switching code; dyld selects the matching dylib slice
automatically when cores are loaded.

Rejected alternatives:
- **Two side-by-side `.app`s** (RetroArch's model): more files to ship, no
  user benefit at the cost of two install/update paths.
- **RetroNest self-relaunches under Rosetta on PS2 launch**: more moving
  parts (state preservation, `execvp("arch")` plumbing) for an automation
  win the user can achieve in two clicks.
- **Drop arm64 entirely**: penalizes mGBA and RetroNest's UI runtime for
  the sake of PCSX2 only; user explicitly wanted to preserve native paths
  where they already work.

## Core-architecture rule (codified policy)

**All libretro cores RetroNest ships are universal binaries**, even when a
core's arm64 slice has no perf advantage (e.g., PCSX2 today). The rule
eliminates a whole class of host/core arch-mismatch failure modes (dlopen
fails cryptically when host and core don't match) and means the user can
flip the Get-Info toggle freely without thinking about which cores
work in which mode.

In scope for SP10: PCSX2 (`pcsx2_libretro.dylib`) and mGBA
(`mgba_libretro.dylib`) — the two cores RetroNest currently ships. The
policy is also added to `RetroNest-Project/CLAUDE.md` so future sub-projects
(DuckStation libretro, PPSSPP libretro) inherit it.

## Build pipeline

### Dependency strategy: dual Homebrew

`/opt/homebrew` stays as today (arm64). A parallel x86_64 Homebrew is
installed at `/usr/local` via the official one-liner:

```sh
arch -x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

`/usr/local/bin/brew` is then used (always under `arch -x86_64`) to install
x86_64 slices of every dependency: `qt@6`, `sdl2`, plus PCSX2's deps
(`libwebp`, `libzip`, `lz4`, `fmt`, `soundtouch`, etc. — full list
authoritatively defined by `pcsx2-master/.github/workflows/macos-build.yml`).

A new `scripts/setup-x86_64-toolchain.sh` in RetroNest-Project automates
the one-time installation and is idempotent (`brew list ... || brew install`).

Rationale (vs. universal Qt installer or full from-source superbuild):
matches PCSX2 nightlies' build approach; well-trodden path; lowest
maintenance after setup; doesn't require giving up Homebrew's update
ergonomics.

### Per-target build flow

Each universal artifact is built twice (once per arch) and `lipo`-merged.

**RetroNest:**

```sh
# arm64 slice
arch -arm64 cmake -S cpp -B cpp/build-arm64 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/sdl2"
arch -arm64 cmake --build cpp/build-arm64

# x86_64 slice
arch -x86_64 cmake -S cpp -B cpp/build-x86_64 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2"
arch -x86_64 cmake --build cpp/build-x86_64
```

`pcsx2_libretro.dylib` and `mgba_libretro.dylib`: same pattern, single
`.dylib` output instead of a `.app`.

### Bundling Qt + SDL2 inside the `.app`

Today RetroNest links Qt at `/opt/homebrew/opt/qt/lib` via `@rpath` and
relies on Homebrew being installed at runtime. Under Rosetta this breaks:
dyld would try to load arm64 Qt frameworks into an x86_64 process. The
pipeline therefore bundles Qt + SDL2 inside the `.app`, lipo-merged to
universal.

Approach: run `macdeployqt` against each arch's `.app` independently
(invoked with the Qt prefix matching that arch), then `lipo`-merge the
two resulting `Contents/Frameworks/` trees in the merge script.

### Merge script: `scripts/lipo-merge-app.sh`

1. `cp -R cpp/build-arm64/RetroNest.app cpp/build-universal/RetroNest.app`
   — start from the arm64 bundle as the structural template.
2. Walk both bundles in parallel: for every `Contents/MacOS/<exec>` and
   every `.dylib` / `.framework/.../<binary>` discovered, run
   `lipo -create build-arm64/... build-x86_64/... -output build-universal/...`.
3. For non-Mach-O files (Info.plist, resources, qml/, themes/), copy from
   arm64 with a `diff` sanity check against the x86_64 build. Diverging
   non-Mach-O files indicate a build-system bug and fail the merge loudly.
4. Codesign the merged `.app` with the entitlements file (see below).
5. `lsregister -f cpp/build-universal/RetroNest.app` — moved here from the
   per-arch POST_BUILD step (Launch Services should point at the universal
   bundle, not either slice).

A separate `scripts/lipo-merge-dylib.sh` does the simpler dylib version
(single-file lipo + codesign).

`scripts/build-universal.sh` orchestrates everything (preflight dep check
on both prefixes, two builds, merge, install to
`~/Documents/RetroNest/emulators/libretro/cores/`).

## Code signing & entitlements

PCSX2's x86_64 recompilers (`recCpu`, `psxRec`, `CpuMicroVU0/1`) allocate
executable memory at runtime via `mmap(PROT_EXEC)` / `mprotect`. Under
macOS hardened runtime (implicit under Rosetta), these return `EPERM`
unless the **host process** is signed with JIT entitlements. The JIT
runs in-process (via `dlopen` of `pcsx2_libretro.dylib`), so the
entitlements go on RetroNest's binary, not the dylib.

New file `cpp/resources/RetroNest.entitlements`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>com.apple.security.cs.allow-jit</key>
    <true/>
    <key>com.apple.security.cs.allow-unsigned-executable-memory</key>
    <true/>
    <key>com.apple.security.cs.disable-library-validation</key>
    <true/>
</dict>
</plist>
```

These match standalone PCSX2 v2.6.3.app's entitlements exactly (verified
via `codesign -d --entitlements -`). Brief justification per entitlement:

- `allow-jit` — primary, enables the modern `MAP_JIT` recompiler path.
- `allow-unsigned-executable-memory` — older/broader; upstream PCSX2 keeps
  it for safety, we follow suit.
- `disable-library-validation` — required for `dlopen` of our
  locally-signed (or ad-hoc-signed) libretro dylibs from a host signed
  with a different identity. Without this, dyld rejects the load.

Dev-time signing (the only path we care about for SP10):

```sh
codesign --force --deep --options runtime \
    --entitlements cpp/resources/RetroNest.entitlements \
    --sign - \
    cpp/build-universal/RetroNest.app
```

`--sign -` is ad-hoc (no Developer ID needed). `--options runtime`
enables hardened runtime, which is required for the entitlements to take
effect.

**Info.plist constraint:** the bundle must **not** set
`LSRequiresNativeExecution=true`. That key forces native arm64 and
disables the Get-Info "Open using Rosetta" toggle, defeating the entire
SP10 switch model. The implementation plan includes a one-line assertion
in the merge script that this key is absent from the merged Info.plist.

## RetroNest C++ changes

### 1. Host-arch detection helper

New `cpp/src/core/platform/host_arch.h` (compile-time, ~20 lines):

```cpp
namespace HostArch {
    constexpr bool isArm64() {
    #if defined(__aarch64__)
        return true;
    #else
        return false;
    #endif
    }
    constexpr bool isRosettaX86_64() { return !isArm64(); }
}
```

Compiles per-slice. On Apple Silicon hardware (the only target), an
x86_64 process means Rosetta by definition — no `sysctl.proc_translated`
runtime check needed.

### 2. Slow-mode toast at PS2 launch

In the PS2 libretro launch path (`GameSession::start` or equivalent), when
the target system is `ps2` and `HostArch::isArm64()` is true, emit a
non-blocking toast through the existing overlay/toast plumbing
(`LibretroOverlayPanel` already routes RA toasts — the implementation
plan picks the exact API; the snippet below is illustrative):

```cpp
if (system == "ps2" && HostArch::isArm64() && !m_slowModeToastDismissed) {
    m_overlayPanel->showToast(
        "PS2 emulation is faster under Rosetta. "
        "Quit, then right-click RetroNest in Finder → Get Info → "
        "tick \"Open using Rosetta\", and relaunch.",
        ToastKind::Info,
        ToastDuration::Long);
    m_slowModeToastDismissed = true;
}
```

Dismissal flag (`m_slowModeToastDismissed`) lives on `AppController`,
defaults to `false`, resets on each RetroNest launch. Non-blocking — the
game still launches and runs at ~65–70%. One toast per session.

### 3. Memory / docs deliverables

- Append to `RetroNest-Project/CLAUDE.md` under "Build & Run": brief
  pointer to `scripts/build-universal.sh` and the dual-Homebrew model.
- Add (in CLAUDE.md or a new short doc): "All libretro cores RetroNest
  ships are universal binaries — host/core arch-mismatch failures are
  prevented by construction."
- Update auto-memory `pcsx2_libretro_port.md` SP10 entry from `⏳` to `🔄`
  (in progress) when the plan starts, `✅` when shipped.

### What does NOT change

- `Pcsx2LibretroAdapter` and `MgbaLibretroAdapter` — dlopen selects the
  matching slice automatically.
- `core_loader.cpp` — no slice-selection code needed.
- PCSX2 upstream files — the recompilers are already gated on
  `#ifdef _M_X86`. No new upstream-file deviations beyond SP4 + SP5.

## Test plan

### 5.1 Build-artifact verification (mechanical, scripted as `scripts/verify-universal.sh`)

```sh
file cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest
  # → Mach-O universal binary with 2 architectures: [x86_64] [arm64]
file <each .dylib in ~/Documents/RetroNest/emulators/libretro/cores/>
  # → universal, 2 archs each
lipo -info <every Mach-O inside Contents/Frameworks/>
  # → universal each
codesign -d --entitlements - cpp/build-universal/RetroNest.app
  # → must include allow-jit, allow-unsigned-executable-memory,
  #    disable-library-validation
codesign -v cpp/build-universal/RetroNest.app  # exit 0
```

### 5.2 Native arm64 smoke (regression gate)

"Open using Rosetta" unchecked.
- RetroNest launches; UI renders.
- mGBA boots a known-good GBA game, full speed.
- PCSX2 boots R&C 2 to the SP5-verified memory-card prompt; speed ~65–70%
  via the reinstated `OnPerformanceMetricsUpdated` diagnostic (HostStubs.cpp,
  ~20-line drop-in from the SP5 handoff knowledge — revert after testing).
- New: the "switch to Rosetta" toast appears at PS2 launch. Dismiss it;
  confirm it does not reappear within the same session.

### 5.3 Rosetta x86_64 smoke (the new path)

Quit, Finder → Get Info → tick "Open using Rosetta", relaunch.
- Activity Monitor shows the RetroNest process as "Intel".
- UI renders correctly (Qt + SDL2 + Metal-under-Rosetta).
- mGBA loads and runs (universal core's x86_64 slice exercised — first
  real test).
- PCSX2 boots R&C 2 same scene as 5.2.
- The toast does NOT appear.
- Critically: speed approaches 100% (recompilers active under Rosetta).

### 5.4 Three-way perf comparison (the success criterion)

Same R&C 2 save + same scene (intro FMV + first gameplay area). Median
speed/FPS over 60 seconds via `OnPerformanceMetricsUpdated`:

| Configuration                                    | Speed | FPS | EE / GS / VU thread % |
|--------------------------------------------------|-------|-----|------------------------|
| Native arm64 RetroNest + arm64 slice (baseline)  |       |     |                        |
| Rosetta x86_64 RetroNest + x86_64 slice (new)    |       |     |                        |
| Standalone PCSX2 v2.6.3.app (reference)          |       |     |                        |

**Success: row 2 within ±2% of row 3.**

### 5.5 Bonus observation (not gating)

SP3.6 (quit-hang) traces back to PCSX2's interpreter-path
`intSafeExitExecution`. Under Rosetta the EE uses recompilers, which have
a different exit dispatch. If quit happens to "just work" under Rosetta,
note it in the SP10 results — but don't gate SP10 on confirming it.

### Out of scope for SP10 testing

- Save states (SP6).
- Settings push (SP7).
- Analog sticks + L2/R2 + rumble (SP5.5).

## Risks & open questions

### Ranked risks

1. **`macdeployqt` cross-arch behavior — Medium.** The arm64 macdeployqt
   binary inspecting an x86_64 build to bundle x86_64 frameworks from
   `/usr/local/opt/qt` is plausible (macdeployqt walks Mach-O load
   commands, which it can do regardless of its own arch) but not
   guaranteed. Mitigation: the implementation plan has an isolated
   smoke step that runs macdeployqt against the x86_64 build alone and
   confirms the resulting `.app` runs under `arch -x86_64`, before the
   merge script wires it into the pipeline.

2. **CAMetalLayer + isa-swizzled NSPanel overlay under Rosetta — Medium.**
   SP3.5's overlay uses a transparent fullscreen `QQuickWindow` attached
   as a child window with `NSWindowAbove` ordering over a Metal NSView.
   Metal under Rosetta is officially supported, but the swizzle + child-
   window pattern is unusual enough to warrant a real smoke test in 5.3
   rather than an assumption.

3. **Dual-Homebrew drift — Low (persistent).** Easy to `brew upgrade` at
   one prefix and forget the other. Mitigation: `build-universal.sh`
   preflight checks `brew --prefix qt` at both locations and bails if
   versions diverge across prefixes.

4. **Build wall-clock doubles — Low.** ~10–20 min per universal build on
   M4 vs. ~5–10 min today. Acceptable at SP cadence; ccache (already in
   PCSX2's CMake config) makes the second build cheap once both arches
   warm the cache.

5. **Stale arm64-only dylib in the runtime cores dir — Low.** A
   pre-SP10 `pcsx2_libretro.dylib` in `~/Documents/RetroNest/emulators/libretro/cores/`
   would fail to load under Rosetta. Mitigation: `build-universal.sh`
   overwrites the runtime dir as its final step.

### Open questions (deferred to implementation, not blocking spec)

- Merge script discovery vs. explicit allowlist for Mach-O files inside
  `Contents/Frameworks/`. Lean toward discovery + assert-equality on
  non-Mach-O.
- Optional: a tiny arch chip in RetroNest's status corner so the user
  can see the current mode at a glance. Nice-to-have, doesn't gate SP10.

### Non-goals (explicit)

- Notarized distribution builds (ad-hoc signing only).
- Intel-Mac support.
- Auto-relaunching RetroNest under Rosetta when a PS2 game is selected.
- Save states / settings push / analog input (other sub-projects).

## Deliverables checklist

- [ ] `scripts/setup-x86_64-toolchain.sh` (one-time x86_64 Homebrew + deps install, idempotent)
- [ ] `scripts/build-universal.sh` (orchestrator: preflight → two builds → merge → install)
- [ ] `scripts/lipo-merge-app.sh` (universal `.app` merge)
- [ ] `scripts/lipo-merge-dylib.sh` (universal `.dylib` merge)
- [ ] `scripts/verify-universal.sh` (5.1 artifact-verification gate)
- [ ] `cpp/resources/RetroNest.entitlements` (JIT + library-validation)
- [ ] `cpp/src/core/platform/host_arch.h` (compile-time arch flag)
- [ ] Slow-mode toast wired into PS2 launch path (`GameSession` + `AppController`)
- [ ] CLAUDE.md updates (build instructions + universal-cores policy)
- [ ] Three-way perf comparison documented in `docs/superpowers/notes/2026-05-11-sp10-perf-results.md`
- [ ] Auto-memory `pcsx2_libretro_port.md` SP10 status updated to ✅ on completion
