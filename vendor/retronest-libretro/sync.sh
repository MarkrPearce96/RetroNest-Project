#!/usr/bin/env bash
# Sync the canonical retronest-libretro package into the adopting core forks.
# Run after ANY edit to a package file. Regenerates MANIFEST.sha256 first, so
# the canonical tree and every vendored copy always agree.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECTS="$(cd "$HERE/../../.." && pwd)"   # …/Documents/Projects

FILES=(
  libretro.h
  retronest_libretro.h
  retronest_nsview.h
  retronest_nsview.mm
  emit_core_options_v2.h
  README.md
  check-drift.sh
  docs/option-style-guide.md
  docs/deploy-contract.md
)

cd "$HERE"
shasum -a 256 "${FILES[@]}" > MANIFEST.sha256
echo "regenerated MANIFEST.sha256 (${#FILES[@]} files)"

DESTS=(
  "$PROJECTS/duckstation-libretro/src/duckstation-libretro/retronest-libretro"
  "$PROJECTS/pcsx2-libretro/pcsx2-libretro/retronest-libretro"
  "$PROJECTS/dolphin-libretro/Source/Core/DolphinLibretro/retronest-libretro"
)
for dest in "${DESTS[@]}"; do
  if [ ! -d "$(dirname "$dest")" ]; then
    echo "SKIP (fork dir missing): $dest"
    continue
  fi
  mkdir -p "$dest/docs"
  for f in "${FILES[@]}"; do
    cp "$HERE/$f" "$dest/$f"
  done
  cp "$HERE/MANIFEST.sha256" "$dest/MANIFEST.sha256"
  chmod 755 "$dest/check-drift.sh"
  echo "synced -> $dest"
done
