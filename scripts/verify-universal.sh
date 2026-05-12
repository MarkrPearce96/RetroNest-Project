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
# Hard-required: pcsx2_libretro is the SP10 goal.
dylib="$CORES_DIR/pcsx2_libretro.dylib"
test -f "$dylib" || fail "pcsx2_libretro.dylib missing in $CORES_DIR"
file "$dylib" | grep -q "universal binary with 2 architectures" \
    || fail "pcsx2_libretro.dylib not universal"
pass "pcsx2_libretro.dylib is universal"

# Soft check: mgba_libretro is built from the external mGBA source tree
# (upstream mgba-emu/mgba). Warn but don't fail when it isn't universal
# — native arm64 mode still loads it fine. Hard-fail would punish anyone
# who hasn't cloned mGBA locally.
dylib="$CORES_DIR/mgba_libretro.dylib"
if [[ ! -f "$dylib" ]]; then
    echo "  ! mgba_libretro.dylib missing — skipping (non-fatal)"
elif file "$dylib" | grep -q "universal binary with 2 architectures"; then
    pass "mgba_libretro.dylib is universal"
else
    echo "  ! mgba_libretro.dylib is not universal (arm64 only) — won't load under Rosetta"
    echo "    Re-run with BUILD_MGBA=1 (default) after MGBA_SRC is set to upstream mgba-emu."
fi

echo
echo "✓ all artifacts verified universal + entitled"
