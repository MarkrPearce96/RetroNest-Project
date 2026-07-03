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

# Core verification runs FIRST and standalone: the universal .app bundle
# only exists during full build-universal.sh runs, but the manifest-driven
# core checks are useful on the daily-driver setup too (packet 6).
echo "=== verifying libretro cores (manifest-driven core_arch) ==="
# Every manifests/*.json with backend "libretro" declares (optionally) a
# core_arch: universal | x86_64 | arm64. Policy:
#   universal      → hard-fail unless lipo shows both arm64 and x86_64
#   x86_64 / arm64 → warn-only when the installed file lacks that arch
#                    (a universal file satisfies a single-arch declaration)
#   undeclared     → note and skip
#   core missing   → note and skip (not every core is installed everywhere)
for mf in manifests/*.json; do
    info="$(python3 - "$mf" <<'EOF'
import json, sys
m = json.load(open(sys.argv[1]))
print(m.get("backend", "process"))
print(m.get("id", ""))
print(m.get("core_dylib", ""))
print(m.get("core_arch", ""))
EOF
)"
    { read -r backend; read -r emu_id; read -r core_dylib; read -r core_arch; } <<< "$info"

    [[ "$backend" == "libretro" ]] || continue
    if [[ -z "$core_dylib" ]]; then
        echo "  ! $emu_id: libretro manifest without core_dylib — skipping"
        continue
    fi

    dylib="$CORES_DIR/$core_dylib"
    if [[ ! -f "$dylib" ]]; then
        echo "  - $emu_id: $core_dylib not installed — skipping"
        continue
    fi

    archs="$(lipo -archs "$dylib" 2>/dev/null || true)"
    case "$core_arch" in
        universal)
            if [[ "$archs" == *arm64* && "$archs" == *x86_64* ]]; then
                pass "$emu_id: $core_dylib is universal ($archs)"
            else
                fail "$emu_id: manifest declares core_arch=universal but $core_dylib is [$archs]"
            fi
            ;;
        x86_64|arm64)
            if [[ " $archs " == *" $core_arch "* || "$archs" == "$core_arch" ]]; then
                pass "$emu_id: $core_dylib contains declared arch $core_arch ($archs)"
            else
                echo "  ! $emu_id: manifest declares core_arch=$core_arch but $core_dylib is [$archs] — warn-only"
            fi
            ;;
        *)
            echo "  - $emu_id: no core_arch declared — skipping arch check"
            ;;
    esac
done

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

echo
echo "✓ all artifacts verified universal + entitled"
