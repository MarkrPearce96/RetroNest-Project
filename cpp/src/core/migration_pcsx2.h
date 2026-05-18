#pragma once

#include <QString>

// SP8: one-shot retirement of the PCSX2 standalone path.
//
// Sentinel-gated migration that:
//   1. If emulators/pcsx2/ looks like a standalone install (any of
//      portable.txt, .version.json, inis/, resources/, PCSX2-v*.app),
//      moves the whole dir to emulators/.archive/pcsx2-standalone-<ts>/.
//   2. If emulators/pcsx2-libretro/ exists, moves its contents into
//      emulators/pcsx2/ (which is empty after step 1, or already empty
//      if standalone was never installed).
//   3. Touches emulators/.sp8-migrated as the sentinel.
//
// Idempotent: if the sentinel exists, returns true immediately.
// Reads paths via Paths::emulatorsDir() — Paths::setRoot() must have
// been called first.
namespace MigrationPcsx2 {
    /** Returns true on success (including no-op). Returns false on
     *  hard failure (a move failed); caller should log and bail. The
     *  sentinel is NOT touched on failure so the next launch retries. */
    bool runIfNeeded();
}
