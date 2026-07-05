#pragma once
// Packet 7 Stage 2: offline declared-options seeding.
//
// Every core in the suite emits its option table during retro_set_environment
// (spike-verified 2026-07-06 against all five deployed cores), so a sidecar
// can be seeded WITHOUT running a game: dlopen the core, install a capture
// callback, call retro_set_environment, read the table, done.
//
// SAFETY CONTRACT: the prober NEVER calls retro_init and NEVER dlcloses.
// dlopen runs static initializers only; dlclose would run static DESTRUCTORS
// — the exact crash class the pcsx2 quit-wedge work eliminated (~GSTextureCache
// static-dtor crashes). Handles are retained in a process-lifetime cache;
// dlopen refcounting makes a later real session of the same core safe.

#include <QString>
#include <optional>

#include "declared_options.h"

namespace CoreProber {

/** Probe `coreDylibPath` for its declared options. Returns nullopt when the
 *  dylib can't be loaded, lacks the libretro entry points, or declares no
 *  options during retro_set_environment. Serialized internally (mutex) —
 *  probes are rare (fresh install / missing sidecar only). */
std::optional<DeclaredOptionsDoc> probe(const QString& coreDylibPath);

} // namespace CoreProber
