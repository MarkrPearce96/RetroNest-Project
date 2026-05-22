#pragma once

#include <QString>

// One-shot retirement of the PPSSPP standalone path.
//
// Sentinel-gated migration that:
//   1. If emulators/ppsspp/ looks like a standalone install (PPSSPPSDL.app
//      bundle present as canary), removes the .app bundle plus the
//      standalone install metadata (.version.json) in-place.
//   2. Normalises the PSP data directory from PPSSPP's forced uppercase
//      PSP/ layout to the lowercase psp/ layout libretro writes to. On
//      case-insensitive volumes the rename is effectively a no-op (same
//      inode) but a two-step rename via a temp name forces the on-disk
//      casing to match so Finder no longer shows the old name. On
//      case-sensitive volumes with both PSP/ and psp/ present, PSP/
//      contents are merged into psp/.
//   3. Drops psp/SYSTEM/ppsspp.ini and psp/SYSTEM/controls.ini —
//      libretro does not read them so they become orphans. SAVEDATA/,
//      PPSSPP_STATE/, Cheats/, TEXTURES/ are left alone because
//      libretro uses the same paths and formats.
//   4. Touches emulators/.ppsspp-libretro-migrated as the sentinel.
//
// Idempotent: if the sentinel exists, returns true immediately.
// Reads paths via Paths::emulatorsDir() — Paths::setRoot() must have
// been called first.
namespace MigrationPpsspp {
    /** Returns true on success (including no-op). Returns false on
     *  hard failure (a destructive op failed); caller should log and
     *  bail. The sentinel is NOT touched on failure so the next launch
     *  retries. */
    bool runIfNeeded();
}
