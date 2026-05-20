# Extract `HotkeyService` from `ConfigService`

**Date:** 2026-05-20
**Status:** Approved (brainstorming)
**Roadmap item:** Tier 2 #6 (`refactor-roadmap.md`)

## Problem

`cpp/src/services/config_service.cpp` is 886 LOC carrying four unrelated concerns: per-emulator settings, per-emulator paths, per-emulator hotkey bindings, and per-port controller bindings (plus quick-settings + capture formatting + libretro-hotkey sentinel handling). The hotkey slice has the cleanest seam:

- 4 public methods (`hotkeyBindings`, `saveHotkey`, `clearHotkey`, `resetHotkeys`) in `config_service.cpp:583-688`, ~105 LOC.
- 1 static helper `libretroHotkeysIniPath()` (lines 33-37) used **only** by those methods.
- Zero shared state with the rest of `ConfigService`: doesn't touch `m_settingsCache`, `m_settingsCachePath`, `m_inputManager`, or `m_loader` directly (uses `AdapterRegistry::instance()` global instead).
- Only emits `statusMessage` — never emits `configurationReset` (that's emitted by `resetConfiguration` itself).
- One cross-tie: `ConfigService::resetConfiguration()` (line 242) calls `resetHotkeys(emuId)` as part of "reset everything to install defaults".
- Existing dedicated test file: `cpp/tests/test_config_service_libretro_hotkeys.cpp` exercises all 4 methods with `ConfigService cs(/*loader=*/nullptr)` — proving the hotkey slice is already independent of `ManifestLoader`.

## Goal

Move the hotkey slice into a sibling `HotkeyService` class in `cpp/src/services/hotkey_service.{h,cpp}`. `ConfigService` keeps the same QML-facing contract (via `AppController`'s Q_INVOKABLE shims) but no longer owns hotkey logic. Future hotkey work (e.g. promoting libretro hotkeys to a real settings dialog, adding per-game hotkey overrides) edits one focused file instead of carving through a multi-concern monolith.

## Non-goals

- No behavior change. Same INI files, same sentinel `emuId`, same signals, same write semantics. The smoke test should be indistinguishable from pre-refactor.
- No further `ConfigService` decomposition in this PR. The settings/paths/controllers slices stay where they are — separate Tier-2 work if ever needed.
- No new tests. Existing `test_config_service_libretro_hotkeys.cpp` already covers the libretro sentinel path; it gets renamed and re-pointed at `HotkeyService` but the test logic is unchanged.

## Architecture

**`HotkeyService`** owns the four methods and the helper. It has no constructor dependencies — bodies use `AdapterRegistry::instance()` directly and create local `IniFile` instances per call (same pattern as today's `ConfigService` hotkey code, lifted verbatim).

**`ConfigService`** loses its hotkey declarations and bodies but gains a non-owning `HotkeyService*` pointer for its one cross-tie (the `resetConfiguration` reset call). The pointer is constructor-injected so the dependency is explicit.

**`AppController`** owns both services as direct members. Member-declaration order matters: `m_hotkeyService` is declared **before** `m_configService` so it's constructed first and can be passed by pointer into `m_configService`'s constructor via the member initializer list. AppController's four Q_INVOKABLE hotkey shims (currently delegating to `m_configService`) re-target to `m_hotkeyService`. The QML surface is unchanged.

### Service API

`cpp/src/services/hotkey_service.h`:

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class HotkeyService : public QObject {
    Q_OBJECT
public:
    explicit HotkeyService(QObject* parent = nullptr);

    QVariantList hotkeyBindings(const QString& emuId) const;
    void saveHotkey(const QString& emuId, const QString& section,
                    const QString& key, const QString& value);
    void clearHotkey(const QString& emuId, const QString& section,
                     const QString& key);
    void resetHotkeys(const QString& emuId);

signals:
    void statusMessage(const QString& msg);
};
```

### Wiring contract

`AppController` constructor (sketch):

```cpp
AppController::AppController(ManifestLoader* loader, QObject* parent)
    : QObject(parent),
      m_loader(loader),
      m_hotkeyService(this),                          // declared before m_configService
      m_configService(loader, &m_hotkeyService, this) // takes the pointer
{
    connect(&m_configService, &ConfigService::statusMessage, this, &AppController::setStatus);
    connect(&m_configService, &ConfigService::configurationReset, /* existing handler */);
    connect(&m_hotkeyService, &HotkeyService::statusMessage, this, &AppController::setStatus);  // new
    // ...rest unchanged
}
```

`ConfigService::resetConfiguration` (line 242):

```cpp
// before
resetHotkeys(emuId);

// after
m_hotkeyService->resetHotkeys(emuId);
```

Everywhere else, what was `m_configService.<hotkey method>` in AppController becomes `m_hotkeyService.<hotkey method>`. Four shim lines.

## File changes

### Created (2)

- `cpp/src/services/hotkey_service.h` — the class declaration above.
- `cpp/src/services/hotkey_service.cpp` — file-static `libretroHotkeysIniPath()` plus the four methods, bodies copied verbatim from `config_service.cpp:583-688`. The `libretro_hotkeys::kSentinelEmuId` branch in each method stays exactly as written today.

### Modified (5)

- `cpp/src/services/config_service.h` — drop 4 hotkey declarations + comment (lines 53-58); add `HotkeyService* m_hotkeyService` private member; add `HotkeyService* hotkeyService` parameter to constructor.
- `cpp/src/services/config_service.cpp` — delete the 4 hotkey method bodies (lines 583-688) and the `// ── Hotkeys ──` section header; delete file-static `libretroHotkeysIniPath()` (lines 33-37); update constructor to store the new pointer; update `resetConfiguration()` line 242 to call `m_hotkeyService->resetHotkeys(emuId)`.
- `cpp/src/ui/app_controller.h` — add `#include "services/hotkey_service.h"`; add `HotkeyService m_hotkeyService;` member declared *before* `ConfigService m_configService;`.
- `cpp/src/ui/app_controller.cpp` — update constructor initializer list to construct `m_hotkeyService` first and pass `&m_hotkeyService` into `m_configService`'s constructor; add `connect(&m_hotkeyService, &HotkeyService::statusMessage, this, &AppController::setStatus);`; re-target the 4 hotkey Q_INVOKABLE shims (currently at lines 709, 717, 721, 725) from `m_configService.` to `m_hotkeyService.`.
- `cpp/CMakeLists.txt` — add `src/services/hotkey_service.h` and `src/services/hotkey_service.cpp` to the `qt_add_executable` sources list.

### Renamed (1)

- `cpp/tests/test_config_service_libretro_hotkeys.cpp` → `cpp/tests/test_hotkey_service_libretro.cpp` (via `git mv`).
  - Rename fixture class `TestConfigServiceLibretroHotkeys` → `TestHotkeyServiceLibretro`.
  - Swap `#include "services/config_service.h"` for `#include "services/hotkey_service.h"`.
  - Swap every `ConfigService cs(/*loader=*/nullptr);` for `HotkeyService hs;` and every `cs.` for `hs.`.
  - Update `QTEST_MAIN(TestConfigServiceLibretroHotkeys)` → `QTEST_MAIN(TestHotkeyServiceLibretro)`.
  - Update the test's CMakeLists.txt registration (under `cpp/tests/`) to use the new filename and target name.

## Smoke test

The C++ test suite covers the libretro sentinel path; end-to-end behavior on real emulator hotkeys is verified manually.

1. **Build:** `cmake --build cpp/build-x86_64` succeeds.
2. **Tests:** `ctest --test-dir cpp/build-x86_64` passes — specifically the renamed `test_hotkey_service_libretro` target.
3. **Deploy + sign + launch** per `build-cmake-needs-macdeployqt` memory.
4. **Manual end-to-end** (the Hotkeys dialog is per the `hotkey-settings-redesign` memory entry):
   1. Open Settings → Hotkeys for DuckStation. Bindings list populates with current INI values.
   2. Capture a new binding on one row, Save — verify the INI file at `~/Documents/RetroNest/emulators/duckstation/.../<config>.ini` updated.
   3. Clear a binding — verify the INI key is empty/removed.
   4. Reset all hotkeys — verify defaults restored.
   5. Repeat against PPSSPP for the alternate `controllerBindingsConfigFilePath()` path (PPSSPP writes to `controls.ini`).
   6. Open the global Libretro Hotkeys dialog (sentinel `emuId` path). Verify save / clear / reset on `libretro_hotkeys.ini`.
   7. Settings → Manage Emulators → Reset to Defaults on DuckStation. Verify hotkeys also get reset (proves the `m_hotkeyService->resetHotkeys()` call from inside `resetConfiguration` still wires).

If any step regresses, STOP and investigate.

## LOC impact

| Change | Lines |
|------|------|
| New `hotkey_service.h` | +28 |
| New `hotkey_service.cpp` | +115 |
| `config_service.h` shrinkage | −2 |
| `config_service.cpp` shrinkage | −110 |
| `app_controller.{h,cpp}` | +2 |
| `CMakeLists.txt` | +2 |
| Test rename (file content roughly unchanged size) | 0 |
| **Net** | **+35 LOC** |

The win isn't LOC — it's `config_service.cpp` dropping from 886 to ~776 LOC of single-concern code (settings + paths + controllers + quick-settings) plus a focused 143-LOC `HotkeyService` that's easy to reason about and easy to extend without carving through unrelated logic.

## Out of scope

- Decomposing `ConfigService`'s remaining concerns (settings cache, controllers slice). The settings cache and controller-bindings paths share the `m_settingsCache` infrastructure and are intentionally left coupled in this PR.
- Adding new hotkey features (per-game overrides, additional hotkey actions). The extract leaves room for those later but doesn't pre-build for them.

## Follow-ups (none new from this work)

No follow-ups surfaced during brainstorming. The extract is mechanical.
