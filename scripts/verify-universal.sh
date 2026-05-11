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
