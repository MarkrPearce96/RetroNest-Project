# Dolphin libretro RVZ RetroAchievements + serials — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make RetroAchievements identify + unlock on compressed `.rvz` GameCube/Wii discs (and all DiscIO formats), and populate the game serial for those titles, by having the libretro core compute the RA hash + serial via DiscIO and hand them to the host through one new private env call.

**Architecture:** The core (which has DiscIO + rcheevos) computes the rcheevos hash (`AchievementManager::CalculateHash`) and reads the serial (`DiscIO::CreateVolume(path)->GetGameID()`) during `retro_load_game`, then emits `RETRONEST_SET_GAME_IDENTITY` with both. The host stores them; in `beginSession` it identifies the game by the precomputed hash (`rc_client_begin_load_game`) instead of the path-based identify that fails on RVZ; and it lazily writes the serial to the DB. Other libretro cores are untouched (they keep path-based identify; no hash → fallback).

**Tech Stack:** C++20, Dolphin DiscIO + rcheevos (in the core); Qt6, rcheevos `rc_client`, SQLite (in the host). Spec: `docs/superpowers/specs/2026-05-26-dolphin-libretro-rvz-achievements-serials-design.md`.

---

## Context the executor needs

- **Two repos.** Core: `/Users/mark/Documents/Projects/dolphin-libretro` (branch `libretro`). Host: `/Users/mark/Documents/Projects/RetroNest-Project` (branch `main`). Commit to each repo's current branch; no new branches.
- **Core build (both arches, then deploy):**
  - arm64: `cd /Users/mark/Documents/Projects/dolphin-libretro && cmake --build build-libretro --target dolphin_libretro`
  - x86_64: `PATH=/usr/local/bin:$PATH arch -x86_64 /usr/local/bin/cmake --build build-libretro-x86_64 --target dolphin_libretro`
  - deploy: `Source/Core/DolphinLibretro/tools/deploy.sh` (universal lipo + Sys). Use Bash `timeout` 600000 for builds.
- **Host build + redeploy (required after host C++ changes or the app won't launch):**
  - `cd /Users/mark/Documents/Projects/RetroNest-Project && cmake --build cpp/build-x86_64 --target RetroNest --parallel`
  - then `/usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app` and `codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app`.
- **Editor clangd shows false "file not found" errors** for these files (it lacks the CMake include paths). Trust the actual CMake build.
- **Verified APIs (do not change these signatures):**
  - Core: `AchievementManager::CalculateHash(const std::string&)` → rcheevos hash, `"0"` on failure (`Core/AchievementManager.h:124`; needs `#include "Core/AchievementManager.h"`). `DiscIO::CreateVolume(const std::string&)` → `std::unique_ptr<Volume>` (`DiscIO/Volume.h:174`); `Volume::GetGameID()` (`:96`). `USE_RETRO_ACHIEVEMENTS=ON`, so both are compiled into the core.
  - Host: `rc_client_begin_load_game(rc_client_t*, const char* hash, callback, userdata)` (`rc_client.h:282`); `rc_client_begin_identify_and_load_game(client, consoleId, path, data, size, cb, ud)` (`:267`, current). `Database::updateSerialForRomPath` is NEW (Task 4). Core private env ids live in `Source/Core/DolphinLibretro/LibretroEnvironment.h` (core) and `cpp/src/core/libretro/environment_callbacks.h` (host); next free id is `5 | RETRO_ENVIRONMENT_PRIVATE` (= 0x20005).

## File structure

**Core (`dolphin-libretro`):**
- Modify `Source/Core/DolphinLibretro/LibretroEnvironment.h` — add the `RETRONEST_SET_GAME_IDENTITY` id + `RetroNestGameIdentity` struct.
- Modify `Source/Core/DolphinLibretro/LibretroFrontend.cpp` — compute hash + serial in `retro_load_game`, emit the env.

**Host (`RetroNest`):**
- Modify `cpp/src/core/libretro/environment_callbacks.h` — id + `retronest_game_identity` struct + `EnvironmentContext::raHash`/`gameSerial`.
- Modify `cpp/src/core/libretro/environment_callbacks.cpp` — `SET_GAME_IDENTITY` handler.
- Modify `cpp/tests/test_environment_callbacks.cpp` — unit test for the handler.
- Modify `cpp/src/core/libretro/rcheevos_runtime.{h,cpp}` — `beginSession` `raHash` param + `m_pendingHash` + load-by-hash branch.
- Modify `cpp/src/core/libretro/core_runtime.{h,cpp}` — pass `m_envCtx.raHash` to `beginSession`; add `detectedGameSerial()`.
- Modify `cpp/src/core/game_session.{h,cpp}` — `detectedGameSerial()` forwarder.
- Modify `cpp/src/core/database.{h,cpp}` — `updateSerialForRomPath`.
- Modify `cpp/src/services/game_service.cpp` — lazy serial write in the `started` handler.

---

## Task 1: Core — compute hash + serial, emit SET_GAME_IDENTITY

**Files:**
- Modify: `Source/Core/DolphinLibretro/LibretroEnvironment.h`
- Modify: `Source/Core/DolphinLibretro/LibretroFrontend.cpp`

- [ ] **Step 1: Add the private env id + struct to `LibretroEnvironment.h`**

Find the existing `constexpr unsigned RETRONEST_GET_MACOS_NSVIEW = (1u | RETRO_ENVIRONMENT_PRIVATE);` and add below it:

```cpp
// Matches host RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY = (5 | RETRO_ENVIRONMENT_PRIVATE).
// The core CALLS this during retro_load_game to hand the host the game's
// RetroAchievements hash + serial (both computed via DiscIO, so RVZ works).
// data is a RetroNestGameIdentity*; the host copies both strings.
constexpr unsigned RETRONEST_SET_GAME_IDENTITY = (5u | RETRO_ENVIRONMENT_PRIVATE);

struct RetroNestGameIdentity
{
    const char* ra_hash;  // rcheevos hash string, or "" if unavailable
    const char* serial;   // game id e.g. "GZ2P01", or "" if unavailable
};
```

- [ ] **Step 2: Add includes to `LibretroFrontend.cpp`**

Near the existing `#include "Core/..."` lines, add:

```cpp
#include "Core/AchievementManager.h"
#include "DiscIO/Volume.h"
```

- [ ] **Step 3: Emit the identity in `retro_load_game`**

In `retro_load_game`, immediately after the existing guard
`if (!game || !game->path) { … return false; }` (and before the WSI / boot steps), add:

```cpp
    // Compute the RetroAchievements hash + game serial via DiscIO (handles RVZ
    // and every other Dolphin disc format) and hand them to the host BEFORE it
    // identifies the game. The host calls rc_client_begin_load_game with this
    // hash because rcheevos' own path-based identify can't read compressed RVZ;
    // it also lazily stores the serial. Static storage keeps the const char*s
    // valid for the synchronous env_cb call.
    {
        static std::string s_ra_hash;
        static std::string s_serial;
        s_ra_hash = AchievementManager::CalculateHash(game->path);
        if (s_ra_hash == "0")
            s_ra_hash.clear();
        s_serial.clear();
        if (auto volume = DiscIO::CreateVolume(game->path))
            s_serial = volume->GetGameID();

        RetroNestGameIdentity identity{s_ra_hash.c_str(), s_serial.c_str()};
        if (auto cb = DolphinLibretro::Environment::GetEnvironmentCallback())
            cb(RETRONEST_SET_GAME_IDENTITY, &identity);
        DolphinLibretro::Environment::Log(RETRO_LOG_INFO,
            "[GameIdentity] hash=%s serial=%s",
            s_ra_hash.empty() ? "(none)" : s_ra_hash.c_str(),
            s_serial.empty() ? "(none)" : s_serial.c_str());
    }
```

`RETRONEST_SET_GAME_IDENTITY` and `RetroNestGameIdentity` resolve via the already-present `#include "DolphinLibretro/LibretroEnvironment.h"`. (Note: `AchievementManager::CalculateHash` is a static method that internally creates its own DiscIO volume + registers the rcheevos filereader; calling it here is self-contained. If a future build ever has trouble linking it, the fallback is to inline its body — create the volume, `rc_hash_init_custom_filereader`, `rc_hash_generate_from_file` — but use `CalculateHash` for fidelity.)

- [ ] **Step 4: Build arm64**

Run: `cd /Users/mark/Documents/Projects/dolphin-libretro && cmake --build build-libretro --target dolphin_libretro` (timeout 600000)
Expected: compiles + links, exit 0 (pre-existing macOS-version linker warnings only).

- [ ] **Step 5: Build x86_64 + deploy**

Run:
```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
PATH=/usr/local/bin:$PATH arch -x86_64 /usr/local/bin/cmake --build build-libretro-x86_64 --target dolphin_libretro
Source/Core/DolphinLibretro/tools/deploy.sh
```
Expected: x86_64 builds clean; deploy prints a fat `x86_64 arm64` dylib.

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/dolphin-libretro
git add Source/Core/DolphinLibretro/LibretroEnvironment.h Source/Core/DolphinLibretro/LibretroFrontend.cpp
git commit -m "RVZ RA: compute hash + serial via DiscIO, emit SET_GAME_IDENTITY"
```

> Functional verification is Task 5 (needs the host changes + hardware).

---

## Task 2: Host — SET_GAME_IDENTITY env handler (TDD)

**Files:**
- Modify: `cpp/src/core/libretro/environment_callbacks.h`
- Modify: `cpp/src/core/libretro/environment_callbacks.cpp`
- Test: `cpp/tests/test_environment_callbacks.cpp`

- [ ] **Step 1: Add the id, struct, and context fields to `environment_callbacks.h`**

After the `#define RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR (4 | RETRO_ENVIRONMENT_PRIVATE)` line, add:

```cpp
// 0x20005 — RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY
//           The core CALLS this during retro_load_game to hand us the game's
//           RetroAchievements hash + serial, both computed via DiscIO so they
//           work for compressed RVZ (which rcheevos' own path-based hashing
//           can't read). data is a retronest_game_identity*. We copy both
//           strings into the EnvironmentContext. The host then identifies the
//           game by the hash (rc_client_begin_load_game) and lazily stores the
//           serial. Returns false if data is null.
#define RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY (5 | RETRO_ENVIRONMENT_PRIVATE)

struct retronest_game_identity {
    const char* ra_hash;
    const char* serial;
};
```

Inside `struct EnvironmentContext` (e.g. after `memoryMapSet`), add:

```cpp
    // Captured from RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY: the game's RA hash
    // and serial, computed by the core via DiscIO (works for RVZ). raHash drives
    // load-by-hash in RcheevosRuntime::beginSession; gameSerial is written to the
    // DB lazily on launch (the scanner can't read compressed RVZ).
    QByteArray raHash;
    QByteArray gameSerial;
```

- [ ] **Step 2: Write the failing test**

In `cpp/tests/test_environment_callbacks.cpp`, add a new test method to the test class (alongside `testGetBootStatePathOneShot`):

```cpp
    void testSetGameIdentityCopiesHashAndSerial() {
        EnvironmentContext ctx;
        retronest_game_identity id{"abc123def456", "GZ2P01"};
        QVERIFY(environmentDispatch(&ctx, RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY, &id));
        QCOMPARE(ctx.raHash, QByteArray("abc123def456"));
        QCOMPARE(ctx.gameSerial, QByteArray("GZ2P01"));

        // Null fields are tolerated (treated as empty), call still handled.
        EnvironmentContext ctx2;
        retronest_game_identity id2{nullptr, nullptr};
        QVERIFY(environmentDispatch(&ctx2, RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY, &id2));
        QVERIFY(ctx2.raHash.isEmpty());
        QVERIFY(ctx2.gameSerial.isEmpty());

        // Null data pointer → not handled.
        EnvironmentContext ctx3;
        QVERIFY(!environmentDispatch(&ctx3, RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY, nullptr));
    }
```

- [ ] **Step 3: Run the test to verify it FAILS**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-x86_64 --target test_environment_callbacks --parallel && ctest --test-dir cpp/build-x86_64 -R EnvironmentCallbacks --output-on-failure
```
Expected: FAIL — the handler doesn't exist yet, so `environmentDispatch` returns false / `ctx.raHash` stays empty. (If the ctest name differs, run the binary directly: `./cpp/build-x86_64/test_environment_callbacks`.)

- [ ] **Step 4: Implement the handler in `environment_callbacks.cpp`**

In the `environmentDispatch` switch, add a case (next to `SET_MEMORY_MAPS`):

```cpp
        case RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY: {
            const auto* id = static_cast<const retronest_game_identity*>(data);
            if (!id) return false;
            ctx->raHash     = id->ra_hash ? QByteArray(id->ra_hash) : QByteArray();
            ctx->gameSerial = id->serial  ? QByteArray(id->serial)  : QByteArray();
            qInfo() << "[libretro/env] SET_GAME_IDENTITY hash="
                    << (ctx->raHash.isEmpty() ? "(none)" : ctx->raHash)
                    << "serial=" << (ctx->gameSerial.isEmpty() ? "(none)" : ctx->gameSerial);
            return true;
        }
```

- [ ] **Step 5: Run the test to verify it PASSES**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-x86_64 --target test_environment_callbacks --parallel && ctest --test-dir cpp/build-x86_64 -R EnvironmentCallbacks --output-on-failure
```
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/environment_callbacks.h cpp/src/core/libretro/environment_callbacks.cpp cpp/tests/test_environment_callbacks.cpp
git commit -m "RVZ RA: SET_GAME_IDENTITY env handler captures hash + serial (TDD)"
```

---

## Task 3: Host — identify by precomputed hash in beginSession

**Files:**
- Modify: `cpp/src/core/libretro/rcheevos_runtime.h`
- Modify: `cpp/src/core/libretro/rcheevos_runtime.cpp`
- Modify: `cpp/src/core/libretro/core_runtime.cpp`

- [ ] **Step 1: Add the `raHash` parameter to `beginSession` (`rcheevos_runtime.h`)**

Change the `beginSession` declaration to add a `raHash` parameter (place it right after `romPath`):

```cpp
    bool beginSession(const CoreSymbols& syms,
                      const QString& romPath,
                      const QString& raHash,
                      int raConsoleId,
                      const QString& username,
                      const QString& token,
                      bool hardcore,
                      bool encore,
                      const retro_memory_map* memoryMap);
```

Add a pending-hash field next to `m_pendingRomPath` / `m_pendingConsoleId`:

```cpp
    QString m_pendingHash;
```

- [ ] **Step 2: Store the hash + branch in `rcheevos_runtime.cpp`**

In `beginSession`, update the signature to match (add `const QString& raHash,` after `romPath`), and where it currently stores the pending values:

```cpp
    m_pendingRomPath   = romPath;
    m_pendingConsoleId = raConsoleId;
    m_pendingHash      = raHash;
```

In `loginCallback`, replace the existing `rc_client_begin_identify_and_load_game(...)` call with a branch on the precomputed hash:

```cpp
    if (!self->m_pendingHash.isEmpty()) {
        // Core supplied a precomputed rcheevos hash (Dolphin computes it via
        // DiscIO so RVZ works). Identify directly by hash — rc_hash's own
        // path-based read fails on compressed discs.
        rc_client_begin_load_game(client,
            self->m_pendingHash.toUtf8().constData(),
            loadGameCallback, self);
    } else {
        rc_client_begin_identify_and_load_game(client,
            static_cast<uint32_t>(self->m_pendingConsoleId),
            self->m_pendingRomPath.toUtf8().constData(),
            nullptr, 0,
            loadGameCallback, self);
    }
```

(Memory-region init in `beginSession` is unchanged — it still uses `raConsoleId` + the memory map.)

- [ ] **Step 3: Pass the hash from `core_runtime.cpp`**

At the `m_rcheevos.beginSession(...)` call, add the hash argument (from the captured env context) after `m_cfg.romPath`:

```cpp
    m_rcheevos.beginSession(s, m_cfg.romPath, QString::fromUtf8(m_envCtx.raHash),
                            m_cfg.raConsoleId,
                            m_cfg.raUsername, m_cfg.raToken, m_cfg.raHardcore,
                            m_cfg.raEncore, mmap);
```

- [ ] **Step 4: Build the host**

Run: `cd /Users/mark/Documents/Projects/RetroNest-Project && cmake --build cpp/build-x86_64 --target RetroNest --parallel`
Expected: compiles + links, exit 0. (Also run the env test to confirm no regression: `ctest --test-dir cpp/build-x86_64 -R EnvironmentCallbacks`.)

- [ ] **Step 5: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/rcheevos_runtime.h cpp/src/core/libretro/rcheevos_runtime.cpp cpp/src/core/libretro/core_runtime.cpp
git commit -m "RVZ RA: identify game by core-supplied hash (rc_client_begin_load_game)"
```

---

## Task 4: Host — lazily store the RVZ serial

**Files:**
- Modify: `cpp/src/core/database.h`
- Modify: `cpp/src/core/database.cpp`
- Modify: `cpp/src/core/libretro/core_runtime.h`
- Modify: `cpp/src/core/game_session.h`
- Modify: `cpp/src/core/game_session.cpp`
- Modify: `cpp/src/services/game_service.cpp`

- [ ] **Step 1: Add `updateSerialForRomPath` to the database**

In `database.h`, next to `bool updateSerial(int id, const QString& serial);`, add:

```cpp
    // Set the serial for the game at romPath, but only if it currently has none
    // (used to lazily fill serials the scanner couldn't read, e.g. RVZ).
    bool updateSerialForRomPath(const QString& romPath, const QString& serial);
```

In `database.cpp`, mirror the query style of the existing `serialForRomPath` / `updateSerial` (same `m_db`, table `games`, columns `serial` / `rom_path` — read those two methods first and match their exact identifiers/error handling):

```cpp
bool Database::updateSerialForRomPath(const QString& romPath, const QString& serial) {
    QSqlQuery q(m_db);
    q.prepare("UPDATE games SET serial = ? "
              "WHERE rom_path = ? AND (serial IS NULL OR serial = '')");
    q.addBindValue(serial);
    q.addBindValue(romPath);
    if (!q.exec()) {
        qWarning() << "[Database] updateSerialForRomPath failed:" << q.lastError().text();
        return false;
    }
    return true;
}
```

- [ ] **Step 2: Expose the detected serial from `CoreRuntime`**

In `core_runtime.h` (public section), add:

```cpp
    // Game serial reported by the core via SET_GAME_IDENTITY (empty until set).
    QString detectedGameSerial() const { return QString::fromUtf8(m_envCtx.gameSerial); }
```

- [ ] **Step 3: Forward it from `GameSession`**

In `game_session.h` (public section), declare:

```cpp
    // Serial the libretro core detected for the running game (empty if none /
    // non-libretro). Valid once started() has fired.
    QString detectedGameSerial() const;
```

In `game_session.cpp`, implement it by forwarding to the libretro runtime (mirror how other methods reach `m_libretroAdapter->runtime()`):

```cpp
QString GameSession::detectedGameSerial() const {
    if (m_libretroAdapter && m_libretroAdapter->runtime())
        return m_libretroAdapter->runtime()->detectedGameSerial();
    return {};
}
```

- [ ] **Step 4: Write the serial in GameService's `started` handler**

In `game_service.cpp`, the constructor connects `&GameSession::started`. Append to that lambda's body (it already has `m_currentRomPath` set and `m_db` available):

```cpp
        // Lazily fill the DB serial for formats the scanner couldn't read (RVZ:
        // EmulatorAdapter::extractSerial fails on the compressed header). The core
        // reports it via SET_GAME_IDENTITY; updateSerialForRomPath only writes
        // when the stored serial is still empty.
        const QString detectedSerial = m_session.detectedGameSerial();
        if (!detectedSerial.isEmpty())
            m_db->updateSerialForRomPath(m_currentRomPath, detectedSerial);
```

- [ ] **Step 5: Build the host**

Run: `cd /Users/mark/Documents/Projects/RetroNest-Project && cmake --build cpp/build-x86_64 --target RetroNest --parallel`
Expected: compiles + links, exit 0.

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/database.h cpp/src/core/database.cpp cpp/src/core/libretro/core_runtime.h cpp/src/core/game_session.h cpp/src/core/game_session.cpp cpp/src/services/game_service.cpp
git commit -m "RVZ RA: lazily store core-reported serial in the DB on launch"
```

---

## Task 5: Redeploy host + manual acceptance (HUMAN)

No code — deploy everything and run the acceptance matrix. Needs real RVZ ROMs + an RA login.

- [ ] **Step 1: Redeploy the host app**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
/usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app
codesign --force --deep --sign - cpp/build-x86_64/RetroNest.app
```
(The core was already built + deployed in Task 1.)

- [ ] **Step 2: Launch with logging**

```bash
pkill -x RetroNest; \
RETRONEST_DOLPHIN_LOG=1 arch -x86_64 \
  /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-x86_64/RetroNest.app/Contents/MacOS/RetroNest \
  > /tmp/retronest.log 2>&1 &
```

- [ ] **Step 3: Verify RA on a GameCube RVZ**

Load a GC `.rvz` while logged into RA. In `/tmp/retronest.log` confirm:
- `[GameIdentity] hash=<32 hex> serial=GZ2P01` (or the title's id) from the core,
- `[libretro/env] SET_GAME_IDENTITY hash=… serial=…` from the host,
- **no** `rc_client_begin_identify_and_load_game failed: hash generation failed`,
- the achievement set loads. Trigger an early achievement → unlock toast.

- [ ] **Step 4: Verify RA on a Wii RVZ**

Load a Wii `.rvz` (logged into RA). Confirm the hash/serial lines, `rc_libretro_memory_init` success for console 19 (exercises the SP8 MEM2 map), the set loads, and an achievement unlocks.

- [ ] **Step 5: Verify the serial in the library**

After launching the RVZ title once, return to the library (or restart RetroNest) and confirm the game now shows its real serial (e.g. `GZ2P01`) instead of blank, and that resume now keys by serial (a fresh Save & Quit writes `GZ2P01.resume`).

- [ ] **Step 6: Confirm no regression for other cores**

Launch a PCSX2/PPSSPP/mGBA title — it still identifies + earns achievements (those cores send no hash, so the host falls back to path-based identify). No `SET_GAME_IDENTITY` line for them.

---

## Self-review notes

- **Spec coverage:** Part 1 (core hash+serial+env) ↔ Task 1; Part 2 host env handler ↔ Task 2; host load-by-hash ↔ Task 3; lazy serial (`updateSerial`) ↔ Task 4; testing/acceptance ↔ Task 5. Error-handling (empty hash → fallback; empty serial → no write) is built into Tasks 1/3/4.
- **Type consistency:** `RETRONEST_SET_GAME_IDENTITY` (core, `5u | …`) ↔ `RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY` (host, `5 | …`) — same value 0x20005; structs `RetroNestGameIdentity` / `retronest_game_identity` both `{ const char* ra_hash; const char* serial; }`. `beginSession` gains `const QString& raHash` after `romPath` in both the decl (Task 3.1), the definition (3.2), and the caller (3.3). `detectedGameSerial()` defined on `CoreRuntime` (4.2) and forwarded by `GameSession` (4.3), consumed in `GameService` (4.4). `updateSerialForRomPath` declared (4.1 .h) + defined (4.1 .cpp) + called (4.4).
- **Out of scope (per spec):** scan-time RVZ serials, the green boot flash, other cores' identify path — none appear as tasks.
- **TDD:** the env handler (Task 2) is genuinely unit-tested; the hash/serial computation and load-by-hash paths need a real disc + network, so they're covered by Task 5 manual acceptance (the project's established pattern for emulator-dependent paths).
