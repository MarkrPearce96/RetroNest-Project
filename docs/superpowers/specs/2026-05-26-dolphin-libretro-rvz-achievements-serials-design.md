# Dolphin libretro — RVZ RetroAchievements + serials

Follow-on to SP8, surfaced during SP8 hardware acceptance. SP8's RA memory-map work is
correct, but achievements never trigger for the user's library because every ROM is
`.rvz` (Dolphin's compressed disc format) and RetroNest's host-side disc reading can't
parse it. This spec makes achievements work on RVZ (and all DiscIO formats), and as a
paired fix populates the game serial for RVZ titles.

## Problem

RetroNest's shared `RcheevosRuntime` identifies a game by calling
`rc_client_begin_identify_and_load_game(client, consoleId, romPath, nullptr, 0, …)`
(`cpp/src/core/libretro/rcheevos_runtime.cpp:350`). rcheevos opens the file itself and
hashes it via `rc_hash_gamecube` / `rc_hash_wii`, which read the **raw** disc image
(`rc_hash_gamecube` checks the GameCube magic `0xC2339F3D` at file offset `0x1c`). For a
compressed `.rvz`, that offset isn't the magic, so hashing fails:
`rc_client_begin_identify_and_load_game failed: hash generation failed`. No game id → no
achievement set → no unlocks, regardless of the (correct) memory map.

The same root cause makes the host's `EmulatorAdapter::extractSerial`
(`cpp/src/adapters/emulator_adapter.cpp:6`) return empty for `.rvz` (it reads the raw disc
header), so RVZ titles show no serial in the library and resume falls back to the ROM base
name (already handled).

Dolphin's own DiscIO decompresses RVZ fine (the core boots it and reads game id `GZ2P01`).
The fix bridges that capability to the host.

## Key facts (verified)

- **Core can compute the exact RA hash for RVZ.** `AchievementManager::CalculateHash(path)`
  (`dolphin-libretro/Source/Core/Core/AchievementManager.cpp:260`) registers a
  DiscIO-backed `rc_hash` filereader, derives the console id from the volume type, and
  calls `rc_hash_generate_from_file` → the rcheevos hash string (`"0"` on failure). The
  libretro core builds `USE_RETRO_ACHIEVEMENTS=ON`, so `AchievementManager` + rcheevos are
  already compiled into `dolphin_libretro.dylib`.
- **Core can read the serial.** `DiscIO::CreateVolume(path)->GetGameID()`
  (`DiscIO/Volume.h:96`) → e.g. `GZ2P01`, for any DiscIO format.
- **Host can load by precomputed hash.** `rc_client_begin_load_game(client, hash, …)`
  exists (`rcheevos-src/include/rc_client.h:282`).
- **Core→host private-env pattern exists.** `RETRONEST_ENVIRONMENT_PRIVATE` env ids:
  `GET_MACOS_NSVIEW` (0x20001), `GET_BOOT_STATE_PATH` (0x20002), `GET_MEMCARDS_DIR`
  (0x20003), `GET_TEXTURES_DIR` (0x20004). Next free: **0x20005**.
- **Host can update the serial at any time.** `Database::updateSerial(int id, const QString&)`
  (`cpp/src/core/database.h:66`). Serials are normally populated on a scan worker thread
  (`cpp/src/services/game_service.cpp` ~199), but scan runs with **no core loaded**.

## Scope

**In:** RA hash for RVZ/all DiscIO formats so achievements identify + unlock on GameCube
and Wii; lazy population of the game serial (GC/Wii) for RVZ titles.

**Out:** Scan-time serials for un-launched RVZ titles (would require reimplementing RVZ
decompression host-side — see Decision D3). The green boot flash (separate, accepted as
cosmetic). Changes to other cores' identify path (PCSX2/PPSSPP/mGBA keep path-based
identify).

## Architecture

One new private libretro env call. The core, which already has DiscIO + rcheevos, computes
the RA hash and reads the serial during `retro_load_game` and hands both to the host. The
host uses the hash to identify the game (load-by-hash) and updates the DB serial.

```
retro_load_game(game->path)                 [core, dolphin-libretro]
  ├─ hash   = AchievementManager::CalculateHash(game->path)   // DiscIO-backed, RVZ ok
  ├─ serial = DiscIO::CreateVolume(game->path)->GetGameID()
  └─ env_cb(RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY, &{hash, serial})
                                              │
                                              ▼
environment_callbacks                         [host, RetroNest]
  └─ store ctx->raHash, ctx->gameSerial
beginSession(...)
  └─ raHash non-empty ? rc_client_begin_load_game(client, raHash)
                       : rc_client_begin_identify_and_load_game(consoleId, path)  // fallback
after load
  └─ gameSerial non-empty ? Database::updateSerial(gameId, gameSerial)
```

### Core (`dolphin-libretro`, `libretro` branch)

- In `LibretroFrontend.cpp`'s `retro_load_game`, after validating `game->path` and before
  boot, compute the hash and serial and emit the env. Keep the heavy lifting in a small
  helper (e.g. `ReportGameIdentity(path)`); store the two strings in function-static
  `std::string`s so the `const char*`s stay valid for the synchronous env call.
- **Hash:** `AchievementManager::CalculateHash(game->path)`. If it returns `"0"`/empty,
  pass an empty `ra_hash` (host falls back).
- **Serial:** create one `DiscIO::CreateVolume(game->path)`; `GetGameID()`. Empty/failure →
  empty `serial`. (CalculateHash creates its own volume internally; a second short-lived
  volume here for the id is acceptable one-time load cost. If we'd rather avoid two volume
  creations, the helper may instead create one volume, read the id, and compute the hash
  via the same DiscIO filereader pattern — but reusing `CalculateHash` is preferred for
  fidelity with Dolphin's tested hashing.)
- Failure of the whole step is non-fatal: skip the env call; boot proceeds.

### Host (`RetroNest`, `main`)

- **`environment_callbacks.{h,cpp}`:** define `RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY`
  (`5 | RETRO_ENVIRONMENT_PRIVATE`) and a `struct retronest_game_identity { const char*
  ra_hash; const char* serial; }`. Handler copies both into the `EnvironmentContext`
  (`QByteArray raHash`, `QByteArray gameSerial`).
- **`RcheevosRuntime::beginSession`:** add a `const QString& raHash` parameter (threaded
  from `core_runtime` via `m_envCtx.raHash`). If non-empty, call
  `rc_client_begin_load_game(m_client, raHash.toUtf8().constData(), loadGameCallback, this)`;
  else keep the current `rc_client_begin_identify_and_load_game(consoleId, romPath, …)`.
  Memory-region init is unchanged (still uses `raConsoleId` 16/19 + the map).
- **Serial update:** in `core_runtime` after load (where `started()` is emitted), if
  `m_envCtx.gameSerial` is non-empty, resolve the game id for `m_cfg.romPath` and call
  `Database::updateSerial(id, serial)`. Guard so it only writes when the stored serial is
  empty/different. This makes the library show the serial and lets `resumeStateFile` /
  `game_session` key by serial on subsequent launches (the base-name fallback already
  covers the first launch).

## Error handling

- `CalculateHash` → `"0"`/empty (corrupt/unsupported): core sends empty `ra_hash`; host
  uses path-identify (works for raw ISO/GCM; for RVZ it fails as before — no regression,
  just no achievements for that unreadable file).
- `GetGameID` empty: no serial sent; DB unchanged; resume keeps using the base name.
- Env not handled (older host): core's `env_cb` returns false; core logs and proceeds —
  identical to today's behavior.

## Testing

- **Core:** a focused check that, for a sample RVZ, `AchievementManager::CalculateHash`
  returns a non-`"0"` 32-char hash and `GetGameID` a 6-char id (needs a test disc; may be a
  manual/log check via `RETRONEST_DOLPHIN_LOG=1` printing the computed hash+serial).
- **Host:** the `beginSession` branch — with a non-empty hash it calls
  `rc_client_begin_load_game`; with empty it falls back. Verify `updateSerial` runs once.
- **Manual acceptance (the gate):** load an RVZ **GameCube** and an RVZ **Wii** title
  logged into RA → `rc_client` identifies the game (no "hash generation failed"), the
  achievement set loads, and an early achievement unlocks (Wii also exercises the SP8 MEM2
  map). The library shows the real serial (e.g. `GZ2P01`) after first launch.

## Sequencing (informs the plan)

1. Core: `ReportGameIdentity` + the `SET_GAME_IDENTITY` env emit.
2. Host: env handler + context fields.
3. Host: `beginSession` load-by-hash branch.
4. Host: lazy `updateSerial`.
5. Manual acceptance on GC + Wii RVZ.
