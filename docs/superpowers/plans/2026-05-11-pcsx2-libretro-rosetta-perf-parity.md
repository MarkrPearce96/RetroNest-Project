# SP10 — PCSX2 libretro Rosetta perf parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship a universal (`arm64;x86_64`) RetroNest.app and universal libretro cores (PCSX2, mGBA) so the user can flip Finder → Get Info → "Open using Rosetta" and load PCSX2's x86_64 recompilers in-process, closing the ~30% perf gap to standalone PCSX2.

**Architecture:** Build each target twice (once per arch) using a dual Homebrew setup (`/opt/homebrew` arm64 + `/usr/local` x86_64), then `lipo`-merge into universal Mach-O artifacts. Bundle Qt + SDL2 inside `RetroNest.app` via `macdeployqt`. Codesign with JIT entitlements so PCSX2's x86_64 recompilers can `mprotect(PROT_EXEC)` under Rosetta's hardened runtime. Inside RetroNest, a compile-time `HostArch::isArm64()` helper drives a non-blocking toast at PS2 launch when the user is in the slow mode.

**Tech Stack:** macOS Apple Silicon (M-series), Rosetta 2, Homebrew (dual prefix), CMake 3.16+, Qt6, SDL2, `lipo`/`codesign`/`macdeployqt`, bash.

**Repos involved:**
- `/Users/mark/Documents/Projects/RetroNest-Project/` (the host app — this plan's commits land here)
- `/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/` (note the trailing space) on branch `retronest-libretro` — pcsx2_libretro.dylib built here

**Where the merged universal artifacts go:**
- `RetroNest.app` → `cpp/build-universal/RetroNest.app` then installed by the user where they want.
- `pcsx2_libretro.dylib` → `~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib` (overwrites the current arm64 build).
- `mgba_libretro.dylib` → same dir.

---

## File Structure

**New files (RetroNest-Project):**
- `scripts/setup-x86_64-toolchain.sh` — one-time x86_64 Homebrew + deps install
- `scripts/lipo-merge-dylib.sh` — universal `.dylib` merge (libretro cores)
- `scripts/lipo-merge-app.sh` — universal `.app` merge (RetroNest)
- `scripts/build-universal.sh` — top-level orchestrator
- `scripts/verify-universal.sh` — artifact-verification gate
- `cpp/resources/RetroNest.entitlements` — JIT + library-validation entitlements
- `cpp/src/core/platform/host_arch.h` — compile-time arch flag

**Modified files (RetroNest-Project):**
- `cpp/CMakeLists.txt` — wire entitlements into a codesign step (or leave to merge script)
- `cpp/src/core/game_session.cpp` — emit slow-mode toast at PS2-libretro launch
- `cpp/src/ui/app_controller.h` / `.cpp` — add `m_slowModeNoticeShown` dismissal flag if we keep it on the controller; alternatively keep it on GameSession itself
- `CLAUDE.md` — add universal-build instructions + all-cores-universal policy

**Outside RetroNest-Project:**
- `pcsx2-master/` — no source-file edits; build invocations only
- mGBA libretro source — discovered or cloned in Task 11

---

## Task 1: Setup x86_64 Homebrew toolchain

**Files:**
- Create: `scripts/setup-x86_64-toolchain.sh`

- [ ] **Step 1: Create the setup script**

Path: `RetroNest-Project/scripts/setup-x86_64-toolchain.sh`

```bash
#!/usr/bin/env bash
# Idempotent one-time setup for the x86_64 build toolchain.
# Installs a parallel x86_64 Homebrew at /usr/local plus every dep the
# universal RetroNest build needs in x86_64 form. Safe to re-run.
set -euo pipefail

if [[ "$(uname -m)" != "arm64" ]]; then
    echo "This script targets Apple Silicon hosts. Detected $(uname -m)." >&2
    exit 1
fi

if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
    echo "Rosetta 2 is not installed. Run: softwareupdate --install-rosetta" >&2
    exit 1
fi

# 1. Install x86_64 Homebrew at /usr/local if missing.
if [[ ! -x /usr/local/bin/brew ]]; then
    echo "Installing x86_64 Homebrew at /usr/local..."
    arch -x86_64 /bin/bash -c \
        "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo "x86_64 Homebrew already at /usr/local — updating."
    arch -x86_64 /usr/local/bin/brew update
fi

# 2. Install deps idempotently (RetroNest + PCSX2 + mGBA).
# PCSX2 dep list sourced from pcsx2-master/.github/workflows/macos-build.yml.
DEPS=(
    qt@6 sdl2
    fmt libwebp libzip libpng zstd lz4
    soundtouch ffmpeg
    pkg-config cmake ninja
)

for dep in "${DEPS[@]}"; do
    if arch -x86_64 /usr/local/bin/brew list --formula "$dep" >/dev/null 2>&1; then
        echo "  ✓ $dep"
    else
        echo "  → installing $dep (x86_64)"
        arch -x86_64 /usr/local/bin/brew install "$dep"
    fi
done

echo
echo "Done. /usr/local/bin/brew is the x86_64 brew; /opt/homebrew/bin/brew is the arm64 brew."
echo "Run \`scripts/build-universal.sh\` to build."
```

- [ ] **Step 2: Make it executable**

```bash
chmod +x scripts/setup-x86_64-toolchain.sh
```

- [ ] **Step 3: Run it**

```bash
./scripts/setup-x86_64-toolchain.sh
```

Expected: succeeds (even if it takes 5–10 min on first run). On re-run, every dep prints `✓ <name>` and exits within seconds.

- [ ] **Step 4: Verify both brew prefixes have qt**

```bash
ls /opt/homebrew/opt/qt/lib/QtCore.framework/Versions/A/QtCore
ls /usr/local/opt/qt/lib/QtCore.framework/Versions/A/QtCore
file /usr/local/opt/qt/lib/QtCore.framework/Versions/A/QtCore
```

Expected: third line outputs `Mach-O 64-bit dynamically linked shared library x86_64`.

- [ ] **Step 5: Commit**

```bash
git add scripts/setup-x86_64-toolchain.sh
git commit -m "SP10 step 1 — x86_64 Homebrew toolchain setup script"
```

---

## Task 2: Add JIT entitlements file

**Files:**
- Create: `cpp/resources/RetroNest.entitlements`

- [ ] **Step 1: Create the entitlements plist**

Path: `RetroNest-Project/cpp/resources/RetroNest.entitlements`

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

These match standalone PCSX2 v2.6.3.app exactly (verified during brainstorming via `codesign -d --entitlements -`).

- [ ] **Step 2: Validate the plist**

```bash
plutil -lint cpp/resources/RetroNest.entitlements
```

Expected: `cpp/resources/RetroNest.entitlements: OK`.

- [ ] **Step 3: Commit**

```bash
git add cpp/resources/RetroNest.entitlements
git commit -m "SP10 step 2 — JIT + library-validation entitlements"
```

---

## Task 3: Add host_arch.h compile-time helper

**Files:**
- Create: `cpp/src/core/platform/host_arch.h`

- [ ] **Step 1: Create the header**

Path: `RetroNest-Project/cpp/src/core/platform/host_arch.h`

```cpp
#pragma once

// Compile-time host-arch flag. Universal builds compile this header twice
// (once per slice); each slice's binary returns the matching value at
// runtime. On Apple Silicon hardware (the only target), the x86_64 slice
// running means the process is under Rosetta — no sysctl.proc_translated
// runtime check needed.
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

- [ ] **Step 2: Verify it compiles in isolation**

```bash
echo '#include "cpp/src/core/platform/host_arch.h"
int main() { return HostArch::isArm64() ? 0 : 1; }' | \
    c++ -std=c++17 -x c++ -I cpp/src - -o /tmp/host_arch_check && \
    /tmp/host_arch_check && echo "arm64 slice OK"
```

Expected: prints `arm64 slice OK` on Apple Silicon.

- [ ] **Step 3: Verify x86_64 slice compiles too**

```bash
echo '#include "cpp/src/core/platform/host_arch.h"
int main() { return HostArch::isArm64() ? 1 : 0; }' | \
    arch -x86_64 c++ -std=c++17 -arch x86_64 -x c++ -I cpp/src - -o /tmp/host_arch_check_x86 && \
    arch -x86_64 /tmp/host_arch_check_x86 && echo "x86_64 slice OK"
```

Expected: prints `x86_64 slice OK`.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/platform/host_arch.h
git commit -m "SP10 step 3 — HostArch compile-time slice flag"
```

---

## Task 4: lipo-merge-dylib.sh script

**Files:**
- Create: `scripts/lipo-merge-dylib.sh`

- [ ] **Step 1: Create the script**

Path: `RetroNest-Project/scripts/lipo-merge-dylib.sh`

```bash
#!/usr/bin/env bash
# Merge two arch-specific .dylib files into a universal .dylib via lipo,
# then ad-hoc resign it. Used for libretro cores and any standalone dylib.
#
# Usage: lipo-merge-dylib.sh <arm64.dylib> <x86_64.dylib> <output.dylib>
set -euo pipefail

ARM64="$1"
X86_64="$2"
OUT="$3"

if [[ ! -f "$ARM64" ]]; then echo "missing: $ARM64" >&2; exit 1; fi
if [[ ! -f "$X86_64" ]]; then echo "missing: $X86_64" >&2; exit 1; fi

# Sanity: each input must be the arch it claims to be.
file "$ARM64"  | grep -q "arm64"  || { echo "$ARM64 is not arm64"  >&2; exit 1; }
file "$X86_64" | grep -q "x86_64" || { echo "$X86_64 is not x86_64" >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"
lipo -create "$ARM64" "$X86_64" -output "$OUT"

# Ad-hoc resign so dyld validates correctly under hardened runtime.
codesign --force --sign - "$OUT"

# Verify.
file "$OUT" | grep -q "universal binary with 2 architectures" \
    || { echo "lipo output is not universal: $OUT" >&2; exit 1; }
lipo -info "$OUT"
echo "✓ wrote universal $OUT"
```

- [ ] **Step 2: Make executable**

```bash
chmod +x scripts/lipo-merge-dylib.sh
```

- [ ] **Step 3: Smoke test with arbitrary dylibs**

Use Qt's QtCore as a guaranteed-present pair (both archs installed in Task 1):

```bash
./scripts/lipo-merge-dylib.sh \
    /opt/homebrew/opt/qt/lib/QtCore.framework/Versions/A/QtCore \
    /usr/local/opt/qt/lib/QtCore.framework/Versions/A/QtCore \
    /tmp/qtcore-universal-smoke.dylib
file /tmp/qtcore-universal-smoke.dylib
```

Expected last-line output: `Mach-O universal binary with 2 architectures: [x86_64:...] [arm64:...]`.

- [ ] **Step 4: Commit**

```bash
git add scripts/lipo-merge-dylib.sh
git commit -m "SP10 step 4 — lipo-merge-dylib.sh universal dylib merge"
```

---

## Task 5: Build pcsx2_libretro arm64 + x86_64 slices and merge

**Files:**
- No new files. The pcsx2-master CMakeLists already honors `-DCMAKE_OSX_ARCHITECTURES`.

**Working directory for Steps 1–4:** `"/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"` (note trailing space).

- [ ] **Step 1: Build the arm64 slice**

```bash
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
rm -rf build-arm64
arch -arm64 cmake -B build-arm64 \
    -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="/opt/homebrew"
arch -arm64 cmake --build build-arm64 --target pcsx2_libretro -j
```

Expected: builds to `build-arm64/pcsx2-libretro/pcsx2_libretro.dylib`. `file` confirms arm64.

- [ ] **Step 2: Build the x86_64 slice**

```bash
rm -rf build-x86_64
arch -x86_64 cmake -B build-x86_64 \
    -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local"
arch -x86_64 cmake --build build-x86_64 --target pcsx2_libretro -j
```

Expected: builds to `build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib`. `file` confirms x86_64.

- [ ] **Step 3: Merge to universal**

```bash
cd ~/Documents/Projects/RetroNest-Project
./scripts/lipo-merge-dylib.sh \
    "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib" \
    "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib" \
    /tmp/pcsx2_libretro_universal.dylib
file /tmp/pcsx2_libretro_universal.dylib
lipo -archs /tmp/pcsx2_libretro_universal.dylib
```

Expected: `Mach-O universal binary with 2 architectures: [x86_64] [arm64]`, `lipo -archs` prints `x86_64 arm64`.

- [ ] **Step 4: Install to RetroNest's cores dir**

```bash
cp /tmp/pcsx2_libretro_universal.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
file ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
```

Expected: confirms universal binary.

- [ ] **Step 5: No commit needed in this repo (pcsx2-master is a separate repo with its own discipline). The merged dylib lives in `~/Documents/RetroNest/...` outside of any repo.**

---

## Task 6: lipo-merge-app.sh script

**Files:**
- Create: `scripts/lipo-merge-app.sh`

- [ ] **Step 1: Create the script**

Path: `RetroNest-Project/scripts/lipo-merge-app.sh`

```bash
#!/usr/bin/env bash
# Merge two arch-specific .app bundles into a universal .app via lipo.
# Walks both bundles, lipo's every Mach-O, and diffs non-Mach-O for
# consistency. Code-signs the merged bundle with entitlements.
#
# Usage: lipo-merge-app.sh <arm64.app> <x86_64.app> <output.app> <entitlements.plist>
set -euo pipefail

ARM64_APP="$1"
X86_64_APP="$2"
OUT_APP="$3"
ENTITLEMENTS="$4"

if [[ ! -d "$ARM64_APP"  ]]; then echo "missing: $ARM64_APP"  >&2; exit 1; fi
if [[ ! -d "$X86_64_APP" ]]; then echo "missing: $X86_64_APP" >&2; exit 1; fi
if [[ ! -f "$ENTITLEMENTS" ]]; then echo "missing: $ENTITLEMENTS" >&2; exit 1; fi

# 1. Seed output bundle from arm64 (structural template).
rm -rf "$OUT_APP"
cp -R "$ARM64_APP" "$OUT_APP"

# 2. Walk every Mach-O in the arm64 bundle; for each, lipo with its
#    x86_64 counterpart and overwrite in OUT_APP.
cd "$ARM64_APP"
find . -type f \( -perm -u+x -o -name '*.dylib' -o -name '*.so' \) | while read -r relpath; do
    arm_path="$ARM64_APP/$relpath"
    x86_path="$X86_64_APP/$relpath"
    out_path="$OUT_APP/$relpath"

    # Only care about Mach-O files.
    if ! file "$arm_path" 2>/dev/null | grep -q "Mach-O"; then continue; fi
    if [[ ! -f "$x86_path" ]]; then
        echo "  ! x86_64 counterpart missing for $relpath — keeping arm64-only" >&2
        continue
    fi

    lipo -create "$arm_path" "$x86_path" -output "$out_path"
    echo "  ✓ lipo $relpath"
done

cd - >/dev/null

# 3. Diff non-Mach-O files between the two bundles. Any divergence is a
#    build-system bug.
echo
echo "Comparing non-Mach-O files for unexpected divergence..."
diff -r "$ARM64_APP" "$X86_64_APP" \
    --exclude='*.dylib' --exclude='*.so' \
    | grep -v "^Binary files" \
    | grep -v "^Common subdirectories" \
    || true

# 4. Ad-hoc codesign the merged bundle with entitlements + hardened runtime.
echo
echo "Codesigning merged bundle..."
codesign --force --deep --options runtime \
    --entitlements "$ENTITLEMENTS" \
    --sign - \
    "$OUT_APP"

# 5. Refresh Launch Services so `open` picks up the new bundle.
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister \
    -f "$OUT_APP" || true

# 6. Verify.
file "$OUT_APP/Contents/MacOS/$(basename "$OUT_APP" .app)" \
    | grep -q "universal binary with 2 architectures" \
    || { echo "main executable is not universal" >&2; exit 1; }
codesign -d --entitlements - "$OUT_APP" 2>&1 | grep -q "allow-jit" \
    || { echo "entitlements not applied" >&2; exit 1; }

echo "✓ wrote universal $OUT_APP"
```

- [ ] **Step 2: Make executable**

```bash
chmod +x scripts/lipo-merge-app.sh
```

- [ ] **Step 3: Commit (script alone — no smoke test yet; depends on the builds in Tasks 7-9)**

```bash
git add scripts/lipo-merge-app.sh
git commit -m "SP10 step 5 — lipo-merge-app.sh universal app merge + codesign"
```

---

## Task 7: Build RetroNest arm64 slice (clean baseline)

**Files:** No source edits. Just exercises the existing build.

- [ ] **Step 1: Clean arm64 build**

```bash
cd ~/Documents/Projects/RetroNest-Project
rm -rf cpp/build-arm64
arch -arm64 cmake -S cpp -B cpp/build-arm64 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/sdl2"
arch -arm64 cmake --build cpp/build-arm64 -j
```

Expected: builds `cpp/build-arm64/RetroNest.app`.

- [ ] **Step 2: Verify arch**

```bash
file cpp/build-arm64/RetroNest.app/Contents/MacOS/RetroNest
```

Expected: `Mach-O 64-bit executable arm64`.

- [ ] **Step 3: No commit (no source changes, just baseline verification)**

---

## Task 8: Build RetroNest x86_64 slice

- [ ] **Step 1: x86_64 build**

```bash
cd ~/Documents/Projects/RetroNest-Project
rm -rf cpp/build-x86_64
arch -x86_64 cmake -S cpp -B cpp/build-x86_64 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2"
arch -x86_64 cmake --build cpp/build-x86_64 -j
```

Expected: builds `cpp/build-x86_64/RetroNest.app` (may take longer on first run — full Qt link).

- [ ] **Step 2: Verify arch**

```bash
file cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest
```

Expected: `Mach-O 64-bit executable x86_64`.

- [ ] **Step 3: Smoke-launch under Rosetta (sanity, NOT a feature test)**

```bash
arch -x86_64 cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest --help 2>&1 | head -5
```

Expected: the binary runs (whatever it prints — not crashing is the gate). If it crashes here, the toolchain has issues and the plan stalls until they're root-caused.

- [ ] **Step 4: No commit (no source changes)**

---

## Task 9: Bundle Qt + SDL2 into each .app via macdeployqt, then lipo-merge

This is the most-likely-to-bite step (per Risk #1 in the spec). Validate each arch standalone before merging.

- [ ] **Step 1: macdeployqt arm64 build**

```bash
/opt/homebrew/opt/qt/bin/macdeployqt cpp/build-arm64/RetroNest.app -qmldir=qml
ls cpp/build-arm64/RetroNest.app/Contents/Frameworks/ | head -10
file cpp/build-arm64/RetroNest.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore
```

Expected: `Frameworks/` now contains Qt frameworks; `file` shows arm64.

- [ ] **Step 2: macdeployqt x86_64 build (cross-arch macdeployqt is the risk here)**

```bash
/usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=qml
file cpp/build-x86_64/RetroNest.app/Contents/Frameworks/QtCore.framework/Versions/A/QtCore
```

Expected: `Frameworks/QtCore` is x86_64.

**If macdeployqt for x86_64 fails or produces arm64 frameworks**: run it explicitly via `arch -x86_64`:

```bash
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=qml
```

- [ ] **Step 3: Verify x86_64 bundle is self-contained (no dyld fallback to /opt/homebrew)**

```bash
otool -L cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest | grep -E "(homebrew|/usr/local)" | head
```

Expected: all Qt/SDL2 references resolve to `@rpath` or `@executable_path/../Frameworks/...`, NOT to absolute `/opt/homebrew/...` paths. If absolute `/opt/homebrew/...` paths appear, macdeployqt didn't rewrite them; rerun macdeployqt with `-always-overwrite`.

- [ ] **Step 4: lipo-merge the two .apps**

```bash
rm -rf cpp/build-universal/RetroNest.app
mkdir -p cpp/build-universal
./scripts/lipo-merge-app.sh \
    cpp/build-arm64/RetroNest.app \
    cpp/build-x86_64/RetroNest.app \
    cpp/build-universal/RetroNest.app \
    cpp/resources/RetroNest.entitlements
```

Expected: script prints `✓ wrote universal cpp/build-universal/RetroNest.app`.

- [ ] **Step 5: Final verification**

```bash
file cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest
codesign -d --entitlements - cpp/build-universal/RetroNest.app 2>&1 | grep allow-jit
codesign -v cpp/build-universal/RetroNest.app && echo "signature OK"
```

Expected: universal binary with 2 architectures; `allow-jit` line printed; `signature OK`.

- [ ] **Step 6: Manual launch test — arm64 mode**

```bash
open cpp/build-universal/RetroNest.app
# In another terminal:
ps aux | grep -i "RetroNest" | grep -v grep
```

Expected: RetroNest UI appears; `ps` shows the process. (Kind isn't shown by `ps`; use Activity Monitor to verify Kind=Apple if needed.)

Quit RetroNest. Then:

- [ ] **Step 7: Manual launch test — Rosetta mode**

```bash
# Toggle Open using Rosetta via Finder Get-Info, OR force via command:
arch -x86_64 cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest
```

Expected: RetroNest UI appears under Rosetta. The fact that this works at all proves Qt-under-Rosetta + the merged bundle's x86_64 slice are both healthy.

Quit RetroNest.

- [ ] **Step 8: No commit yet (no source changes; results captured later in Task 14)**

---

## Task 10: Slow-mode toast trigger in GameSession

**Files:**
- Modify: `cpp/src/core/game_session.cpp` (add toast emit near `startLibretro` entry)
- Modify: `cpp/src/core/game_session.h` (add `m_slowModeNoticeShown` member; check current header path)
- Modify: `cpp/CMakeLists.txt` (add `cpp/src/core/platform/host_arch.h` to sources only if non-header-only inclusion is needed — header-only, so likely no CMake edit)

- [ ] **Step 1: Read the current GameSession.h to find the right place for the member**

```bash
grep -n "private:" cpp/src/core/game_session.h | head -3
grep -n "m_libretroAdapter" cpp/src/core/game_session.h | head -3
```

Locate an existing bool member in the private section to anchor the edit.

- [ ] **Step 2: Add member to GameSession.h**

Find the private section (existing bool fields nearby, e.g. `m_libretroFastForward`). Add:

```cpp
// SP10: gate the "switch to Rosetta for full speed" notice to one
// emission per RetroNest session.
bool m_slowModeNoticeShown = false;
```

Place it next to existing bool members; matching style.

- [ ] **Step 3: Add the toast emit at the top of startLibretro**

Open `cpp/src/core/game_session.cpp` around line 128 (the start of the libretro-launch function — confirm by searching for `dynamic_cast<LibretroAdapter*>`). Just after the `m_libretroAdapter = lr;` line (~line 134), add:

```cpp
// SP10: warn the user when launching the PS2 libretro core in arm64
// mode — they'll get the interpreter ceiling instead of recompiler
// speed. Dismissable per session. Reuses the existing generic info
// toast plumbing (raInfoToast → RAService → AppController → QML
// AchievementToast component).
if (HostArch::isArm64() && lr->coreId() == "pcsx2" && !m_slowModeNoticeShown) {
    m_slowModeNoticeShown = true;
    emit raInfoToast(
        QStringLiteral("Performance"),
        QStringLiteral("PS2 emulation is faster under Rosetta. "
                       "Quit, right-click RetroNest in Finder → Get Info → "
                       "tick \"Open using Rosetta\", and relaunch."),
        QString(),   // imageUrl — copy whichever empty-default RAService uses;
                     // adjust to match the exact raInfoToast signature
        8000);       // duration ms — also adjust if signature differs
}
```

**Before committing,** verify the `raInfoToast` signature in `cpp/src/core/game_session.h` (search for `void raInfoToast`) and match the exact arg order/types.

- [ ] **Step 4: Add the include**

At the top of `cpp/src/core/game_session.cpp`, with the other `core/` includes:

```cpp
#include "core/platform/host_arch.h"
```

- [ ] **Step 5: Build**

```bash
arch -arm64 cmake --build cpp/build-arm64 -j
```

Expected: clean build, no errors. If `raInfoToast` signature mismatch surfaces, fix the call in Step 3 to match.

- [ ] **Step 6: Verify on-launch behavior in arm64 mode**

```bash
open cpp/build-arm64/RetroNest.app
```

Then launch a PS2 game through the UI. The toast should appear stating "PS2 emulation is faster under Rosetta..." once. Launch a second PS2 game in the same session — toast must NOT reappear.

Quit RetroNest. Relaunch. Launch a PS2 game — toast should appear again (per-session reset).

- [ ] **Step 7: Negative test in Rosetta mode (use Task 9's universal build)**

```bash
arch -x86_64 cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest
```

Launch a PS2 game. The toast must NOT appear (host is x86_64).

- [ ] **Step 8: Commit**

```bash
git add cpp/src/core/game_session.h cpp/src/core/game_session.cpp
git commit -m "SP10 step 6 — slow-mode toast at PS2-libretro launch in arm64 mode"
```

---

## Task 11: Build mGBA libretro slices and merge to universal

**Files:** None in RetroNest-Project. mGBA libretro is built from an external source tree.

- [ ] **Step 1: Locate or clone mGBA libretro source**

```bash
# Look for an existing local mGBA clone first.
find ~/Documents -maxdepth 4 -type d -name "mgba*" 2>/dev/null
```

If a tree exists (likely under `~/Documents/Projects/` or similar), use it. If not, clone fresh:

```bash
cd ~/Documents/Projects
git clone https://github.com/libretro/mgba.git mgba-libretro
cd mgba-libretro
```

- [ ] **Step 2: Build arm64 slice**

```bash
cd <mgba-libretro-source>
rm -rf build-arm64
arch -arm64 cmake -B build-arm64 \
    -DBUILD_LIBRETRO=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF \
    -DCMAKE_OSX_ARCHITECTURES=arm64
arch -arm64 cmake --build build-arm64 --target mgba_libretro -j
file build-arm64/mgba_libretro.dylib
```

Expected: arm64 dylib produced.

- [ ] **Step 3: Build x86_64 slice**

```bash
rm -rf build-x86_64
arch -x86_64 cmake -B build-x86_64 \
    -DBUILD_LIBRETRO=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local"
arch -x86_64 cmake --build build-x86_64 --target mgba_libretro -j
file build-x86_64/mgba_libretro.dylib
```

Expected: x86_64 dylib produced.

- [ ] **Step 4: lipo-merge and install**

```bash
cd ~/Documents/Projects/RetroNest-Project
./scripts/lipo-merge-dylib.sh \
    <mgba-libretro-source>/build-arm64/mgba_libretro.dylib \
    <mgba-libretro-source>/build-x86_64/mgba_libretro.dylib \
    /tmp/mgba_libretro_universal.dylib
cp /tmp/mgba_libretro_universal.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/mgba_libretro.dylib
file ~/Documents/RetroNest/emulators/libretro/cores/mgba_libretro.dylib
```

Expected: universal dylib in the cores dir.

- [ ] **Step 5: Smoke test mGBA in both modes**

Launch the universal RetroNest from Task 9 in arm64 mode → boot a known-good GBA game → verify it runs at full speed (no regression).

Quit. Launch under Rosetta → boot the same GBA game → verify it runs (universal core's x86_64 slice loaded).

- [ ] **Step 6: No commit (no source-tree changes; mGBA build dir is outside RetroNest-Project)**

---

## Task 12: build-universal.sh orchestrator

**Files:**
- Create: `scripts/build-universal.sh`

- [ ] **Step 1: Create the script**

Path: `RetroNest-Project/scripts/build-universal.sh`

```bash
#!/usr/bin/env bash
# Build a universal RetroNest.app + every shipped libretro core, then
# install cores to ~/Documents/RetroNest/emulators/libretro/cores/.
#
# Prerequisites: ./scripts/setup-x86_64-toolchain.sh has been run.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Paths to source trees outside this repo.
PCSX2_SRC="/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
# mGBA: edit if your local clone lives elsewhere.
MGBA_SRC="${MGBA_SRC:-$HOME/Documents/Projects/mgba-libretro}"

CORES_DIR="$HOME/Documents/RetroNest/emulators/libretro/cores"

# 1. Preflight: both brew prefixes have qt.
echo "=== preflight ==="
test -f /opt/homebrew/opt/qt/lib/QtCore.framework/Versions/A/QtCore \
    || { echo "arm64 Qt missing — run scripts/setup-x86_64-toolchain.sh"; exit 1; }
test -f /usr/local/opt/qt/lib/QtCore.framework/Versions/A/QtCore \
    || { echo "x86_64 Qt missing — run scripts/setup-x86_64-toolchain.sh"; exit 1; }

# Optional: warn on version drift between prefixes.
arm_qt="$(/opt/homebrew/bin/brew --prefix qt@6 2>/dev/null | xargs -I{} basename {})"
x86_qt="$(arch -x86_64 /usr/local/bin/brew --prefix qt@6 2>/dev/null | xargs -I{} basename {})"
[[ "$arm_qt" == "$x86_qt" ]] || echo "  ! Qt prefix drift: arm64=$arm_qt x86_64=$x86_qt (continuing)"

# 2. Build RetroNest.app (arm64 + x86_64).
echo "=== building RetroNest arm64 ==="
arch -arm64 cmake -S cpp -B cpp/build-arm64 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/sdl2"
arch -arm64 cmake --build cpp/build-arm64 -j
/opt/homebrew/opt/qt/bin/macdeployqt cpp/build-arm64/RetroNest.app -qmldir=qml

echo "=== building RetroNest x86_64 ==="
arch -x86_64 cmake -S cpp -B cpp/build-x86_64 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2"
arch -x86_64 cmake --build cpp/build-x86_64 -j
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=qml

echo "=== merging RetroNest.app ==="
./scripts/lipo-merge-app.sh \
    cpp/build-arm64/RetroNest.app \
    cpp/build-x86_64/RetroNest.app \
    cpp/build-universal/RetroNest.app \
    cpp/resources/RetroNest.entitlements

# 3. Build pcsx2_libretro.dylib (arm64 + x86_64).
echo "=== building pcsx2_libretro arm64 ==="
( cd "$PCSX2_SRC" && arch -arm64 cmake -B build-arm64 \
    -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="/opt/homebrew" \
  && arch -arm64 cmake --build build-arm64 --target pcsx2_libretro -j )

echo "=== building pcsx2_libretro x86_64 ==="
( cd "$PCSX2_SRC" && arch -x86_64 cmake -B build-x86_64 \
    -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local" \
  && arch -x86_64 cmake --build build-x86_64 --target pcsx2_libretro -j )

./scripts/lipo-merge-dylib.sh \
    "$PCSX2_SRC/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib" \
    "$PCSX2_SRC/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib" \
    "$CORES_DIR/pcsx2_libretro.dylib"

# 4. Build mgba_libretro.dylib (arm64 + x86_64) if source tree available.
if [[ -d "$MGBA_SRC" ]]; then
    echo "=== building mgba_libretro arm64 ==="
    ( cd "$MGBA_SRC" && arch -arm64 cmake -B build-arm64 \
        -DBUILD_LIBRETRO=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
      && arch -arm64 cmake --build build-arm64 --target mgba_libretro -j )

    echo "=== building mgba_libretro x86_64 ==="
    ( cd "$MGBA_SRC" && arch -x86_64 cmake -B build-x86_64 \
        -DBUILD_LIBRETRO=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="/usr/local" \
      && arch -x86_64 cmake --build build-x86_64 --target mgba_libretro -j )

    ./scripts/lipo-merge-dylib.sh \
        "$MGBA_SRC/build-arm64/mgba_libretro.dylib" \
        "$MGBA_SRC/build-x86_64/mgba_libretro.dylib" \
        "$CORES_DIR/mgba_libretro.dylib"
else
    echo "  ! mGBA source tree not found at $MGBA_SRC — skipping; "
    echo "    set MGBA_SRC=<path> to enable."
fi

# 5. Run the verification gate.
./scripts/verify-universal.sh

echo
echo "✓ Universal build complete."
echo "  RetroNest.app: cpp/build-universal/RetroNest.app"
echo "  Cores:         $CORES_DIR/"
```

- [ ] **Step 2: Make executable**

```bash
chmod +x scripts/build-universal.sh
```

- [ ] **Step 3: Run it end-to-end**

```bash
./scripts/build-universal.sh
```

Expected: completes, prints `✓ Universal build complete.` Wall-clock 10–20 min on first run, less on rebuilds (ccache).

- [ ] **Step 4: Commit**

```bash
git add scripts/build-universal.sh
git commit -m "SP10 step 7 — build-universal.sh orchestrator"
```

---

## Task 13: verify-universal.sh gate script

**Files:**
- Create: `scripts/verify-universal.sh`

- [ ] **Step 1: Create the script**

Path: `RetroNest-Project/scripts/verify-universal.sh`

```bash
#!/usr/bin/env bash
# Verify the universal RetroNest.app + libretro cores are correctly
# universal-merged, signed, and entitled. Exit 0 on success.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

APP="cpp/build-universal/RetroNest.app"
CORES_DIR="$HOME/Documents/RetroNest/emulators/libretro/cores"

fail() { echo "  ✗ $1" >&2; exit 1; }
pass() { echo "  ✓ $1"; }

echo "=== verifying RetroNest.app ==="
test -d "$APP" || fail "$APP missing"
file "$APP/Contents/MacOS/RetroNest" | grep -q "universal binary with 2 architectures" \
    || fail "RetroNest main binary not universal"
pass "RetroNest is universal (arm64 + x86_64)"

codesign -v "$APP" 2>/dev/null || fail "RetroNest signature invalid"
pass "RetroNest signature valid"

for ent in allow-jit allow-unsigned-executable-memory disable-library-validation; do
    codesign -d --entitlements - "$APP" 2>&1 | grep -q "$ent" \
        || fail "missing entitlement: $ent"
done
pass "all three JIT/library-validation entitlements present"

# Check that LSRequiresNativeExecution is NOT set (would disable Rosetta switch).
if /usr/libexec/PlistBuddy -c "Print :LSRequiresNativeExecution" \
   "$APP/Contents/Info.plist" 2>/dev/null | grep -qi true; then
    fail "LSRequiresNativeExecution=true breaks Rosetta switch — must be absent"
fi
pass "LSRequiresNativeExecution absent (Rosetta toggle preserved)"

echo
echo "=== verifying libretro cores ==="
for core in pcsx2_libretro mgba_libretro; do
    dylib="$CORES_DIR/$core.dylib"
    test -f "$dylib" || fail "$core.dylib missing in $CORES_DIR"
    file "$dylib" | grep -q "universal binary with 2 architectures" \
        || fail "$core.dylib not universal"
    pass "$core.dylib is universal"
done

echo
echo "✓ all artifacts verified universal + entitled"
```

- [ ] **Step 2: Make executable**

```bash
chmod +x scripts/verify-universal.sh
```

- [ ] **Step 3: Run**

```bash
./scripts/verify-universal.sh
```

Expected: exits 0; prints `✓ all artifacts verified universal + entitled`.

- [ ] **Step 4: Commit**

```bash
git add scripts/verify-universal.sh
git commit -m "SP10 step 8 — verify-universal.sh artifact gate"
```

---

## Task 14: Three-way perf measurement

**Files:**
- Temporary: `pcsx2-master/pcsx2-libretro/HostStubs.cpp` (reinstate the SP5-era `OnPerformanceMetricsUpdated` 20-line diagnostic, then revert)
- Create: `docs/superpowers/notes/2026-05-11-sp10-perf-results.md`

- [ ] **Step 1: Reinstate the OnPerformanceMetricsUpdated diagnostic**

In `"/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2-libretro/HostStubs.cpp"`, locate the (currently empty) `Host::OnPerformanceMetricsUpdated()` and add a once-per-second log similar to what SP5 had:

```cpp
#include "VMManager.h"
#include "PerformanceMetrics.h"
#include <chrono>
void Host::OnPerformanceMetricsUpdated() {
    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() < 1000)
        return;
    last = now;
    fprintf(stderr,
        "[perf] speed=%.1f%% fps=%.1f cpu=%.1f%% gs=%.1f%% vu=%.1f%%\n",
        PerformanceMetrics::GetSpeed(),
        PerformanceMetrics::GetFPS(),
        PerformanceMetrics::GetCPUThreadUsage(),
        PerformanceMetrics::GetGSThreadUsage(),
        PerformanceMetrics::GetVUThreadUsage());
}
```

(Exact include paths may vary — use whatever the SP5 commit history shows for the diagnostic if available.)

- [ ] **Step 2: Rebuild universal pcsx2_libretro with the diagnostic**

```bash
./scripts/build-universal.sh
```

- [ ] **Step 3: Capture native-arm64 baseline**

1. Quit RetroNest. Uncheck Finder → Get Info → "Open using Rosetta" on `cpp/build-universal/RetroNest.app`.
2. Launch RetroNest, boot R&C 2, load a save at a chosen reference scene (intro FMV start + first gameplay area).
3. Capture stderr for ~60 s. Note median speed/fps/cpu/gs/vu.

- [ ] **Step 4: Capture Rosetta-x86_64 measurement**

1. Quit. Tick "Open using Rosetta" via Get Info.
2. Launch (now under Rosetta), boot same R&C 2 save at the same scene.
3. Capture stderr for ~60 s. Note median values.

- [ ] **Step 5: Capture standalone reference**

1. Quit RetroNest.
2. Launch `~/Documents/RetroNest/emulators/pcsx2/PCSX2-v2.6.3.app` (the x86_64 standalone — already under Rosetta).
3. Boot the same R&C 2 save, navigate to the same scene.
4. PCSX2's status bar / `Tools → Show OSD` shows the same metrics. Note median values.

- [ ] **Step 6: Write the results doc**

Path: `RetroNest-Project/docs/superpowers/notes/2026-05-11-sp10-perf-results.md`

Template:

```markdown
# SP10 — Three-way R&C 2 perf comparison

**Date:** 2026-05-11
**Hardware:** Apple M4
**ROM / scene:** Ratchet & Clank 2 — intro FMV start through first gameplay area.
**Measurement window:** 60 s median, `OnPerformanceMetricsUpdated`.

| Configuration                                  | Speed % | FPS | CPU % | GS % | VU % |
|------------------------------------------------|---------|-----|-------|------|------|
| Native arm64 RetroNest + arm64 libretro slice  |         |     |       |      |      |
| Rosetta x86_64 RetroNest + x86_64 slice (SP10) |         |     |       |      |      |
| Standalone PCSX2 v2.6.3.app (reference)        |         |     |       |      |      |

**Success criterion:** row 2 within ±2 % of row 3.

**Observations:**
- (notes — e.g., did quit hang under arm64 mode but work under Rosetta?
  Did mGBA arm64 slice show any regression? Did macdeployqt give any
  surprise dyld lookup at first launch?)
```

Fill the table with measured values.

- [ ] **Step 7: Revert the diagnostic**

In `HostStubs.cpp`, remove the body of `OnPerformanceMetricsUpdated` (restore to its empty pre-Task-14 state).

- [ ] **Step 8: Rebuild without the diagnostic and reinstall**

```bash
./scripts/build-universal.sh
```

- [ ] **Step 9: Commit the results doc**

```bash
git add docs/superpowers/notes/2026-05-11-sp10-perf-results.md
git commit -m "SP10 step 9 — three-way R&C 2 perf comparison results"
```

---

## Task 15: CLAUDE.md updates + auto-memory

**Files:**
- Modify: `CLAUDE.md` (build instructions + universal-cores policy)
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-Pcsx2-Experiment-/memory/project_pcsx2_libretro_port.md` (SP10 status)

- [ ] **Step 1: Append universal-build section to CLAUDE.md**

Append under the existing "Build & Run" section:

```markdown
### Universal (Rosetta-capable) builds

For PS2 perf parity with standalone PCSX2 (which only ships x86_64), the
build can produce a universal `arm64;x86_64` RetroNest.app whose x86_64
slice runs PCSX2's recompilers under Rosetta:

```sh
./scripts/setup-x86_64-toolchain.sh   # one-time x86_64 Homebrew + deps
./scripts/build-universal.sh           # full universal build
./scripts/verify-universal.sh          # artifact-verification gate
```

The merged `.app` lands at `cpp/build-universal/RetroNest.app`. Libretro
cores are installed in-place at `~/Documents/RetroNest/emulators/libretro/cores/`.

The user switches between native arm64 and Rosetta x86_64 via Finder →
Get Info → "Open using Rosetta" on the .app. RetroNest does no auto-
relaunching; dyld picks the matching dylib slice automatically.

**Policy:** all libretro cores RetroNest ships are universal binaries.
This eliminates host/core arch-mismatch failure modes. New cores
(future DuckStation libretro, PPSSPP libretro) follow the same rule.
```

- [ ] **Step 2: Update auto-memory**

Open `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-Pcsx2-Experiment-/memory/project_pcsx2_libretro_port.md` and change the SP10 entry from:

```
10. ⏳ **Rosetta perf parity** — ...
```

to:

```
10. ✅ **Rosetta perf parity (SP10)** — DONE. Spec/plan `RetroNest-Project/docs/superpowers/{specs,plans}/2026-05-11-pcsx2-libretro-rosetta-perf-parity*`. RetroNest.app + pcsx2_libretro.dylib + mgba_libretro.dylib all universal. User flips Finder Get Info → "Open using Rosetta" to enter x86_64 mode and load PCSX2's recompilers. Slow-mode toast at PS2 launch in arm64 mode (one-per-session). Three-way perf results in `docs/superpowers/notes/2026-05-11-sp10-perf-results.md`. Dual Homebrew at `/opt/homebrew` + `/usr/local` is the toolchain; `scripts/build-universal.sh` is the entry point.
```

(Adjust phrasing to match the actually-measured outcome from Task 14.)

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "SP10 step 10 — docs: universal-build instructions + cores-universal policy"
```

(Auto-memory file is outside the repo — no `git add` needed; the Write
tool persists it directly.)

---

## Self-Review Summary

**Spec coverage:** every spec section has a corresponding task:

| Spec section                              | Task(s)          |
|-------------------------------------------|------------------|
| Switch model (universal + Get-Info)       | 7, 8, 9          |
| Core-architecture rule (all universal)    | 5, 11, 15        |
| Dependency strategy (dual Homebrew)       | 1                |
| Per-target build flow                     | 5, 7, 8, 12      |
| Bundling Qt + SDL2 (macdeployqt)          | 9                |
| Merge scripts (`lipo-merge-{app,dylib}`)  | 4, 6             |
| Code signing & entitlements               | 2, 9, 13         |
| `Info.plist` LSRequiresNativeExecution    | 13               |
| RetroNest C++: `HostArch` helper          | 3                |
| RetroNest C++: slow-mode toast            | 10               |
| Docs / memory deliverables                | 15               |
| Test plan 5.1 (artifact verification)     | 13               |
| Test plan 5.2 (native arm64 smoke)        | 9, 10            |
| Test plan 5.3 (Rosetta x86_64 smoke)      | 9, 10, 11        |
| Test plan 5.4 (three-way perf)            | 14               |
| Risk #1 (macdeployqt cross-arch)          | 9 (steps 2-3)    |
| Risk #2 (overlay under Rosetta)           | 9 (step 7), 10   |
| Risk #3 (dual-Homebrew drift)             | 12 (preflight)   |

**Placeholder scan:** no TBD/TODO/implement-later. The toast emit's exact `raInfoToast` signature is explicitly flagged as "verify before commit" in Task 10 Step 3 — this is a known unknown that the implementing engineer resolves by reading the existing signal declaration; this is acceptable in a plan, not a placeholder.

**Type consistency:** `HostArch::isArm64()` defined Task 3 + used Task 10. `lipo-merge-dylib.sh` defined Task 4 + used Tasks 5, 11, 12. `lipo-merge-app.sh` defined Task 6 + used Tasks 9, 12. `RetroNest.entitlements` defined Task 2 + used Tasks 6 (script reads path), 9, 12. Method/script names consistent throughout.
