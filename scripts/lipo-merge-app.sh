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
mkdir -p "$(dirname "$OUT_APP")"
rm -rf "$OUT_APP"
cp -R "$ARM64_APP" "$OUT_APP"

# Absolutise paths — the find loop cd's into the arm64 bundle, which
# would invalidate the original relative paths.
ARM64_APP="$(cd "$ARM64_APP" && pwd)"
X86_64_APP="$(cd "$X86_64_APP" && pwd)"
OUT_APP="$(cd "$OUT_APP" && pwd)"
ENTITLEMENTS="$(cd "$(dirname "$ENTITLEMENTS")" && pwd)/$(basename "$ENTITLEMENTS")"

# 2. Walk every Mach-O in the arm64 bundle; for each, lipo with its
#    x86_64 counterpart and overwrite in OUT_APP.
cd "$ARM64_APP"
# Include framework binaries (e.g. QtCore.framework/Versions/A/QtCore)
# even though they're 644 and have no .dylib/.so suffix — the Mach-O
# filter inside the loop drops the non-binary entries (Headers/,
# Resources/, _CodeSignature/).
find . -type f \( \
        -perm -u+x \
        -o -name '*.dylib' \
        -o -name '*.so' \
        -o -path '*.framework/Versions/*/*' \
    \) | while read -r relpath; do
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
