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
