# Retire the standalone `DuckStationAdapter`

**Date:** 2026-06-29
**Status:** Approved — inline (lightweight) execution
**Type:** Dead-code cleanup

## Background

DuckStation runs exclusively through `DuckStationLibretroAdapter` (the in-process libretro core). `adapter_registry.cpp` maps `"duckstation"` → `DuckStationLibretroAdapter`. The older standalone, process-launch `DuckStationAdapter` (`src/adapters/duckstation_adapter.{cpp,h}`) is **never instantiated at runtime** — verified: the only references to the class outside its own definition are four test files. It sits confusingly beside the live libretro adapter and is compiled into ~10 build targets.

## Verification performed (2026-06-29)

- `grep DuckStationAdapter` (excluding `DuckStationLibretroAdapter`): references only in `duckstation_adapter.{cpp,h}` and four test files — **no runtime registration or instantiation**.
- The four tests (`test_format_binding`, `test_hotkey_defs`, `test_duckstation_schema`, `test_duckstation_controller_schema`) each instantiate **only** `DuckStationAdapter`; they freeze contracts of the dead adapter (formatBinding, hotkeyBindingDefs, settingsSchema, controller schema). The "one slot per emulator" comments are historical — these files cover DuckStation only.
- No live coverage is lost: `DuckStationLibretroAdapter` keeps its own `test_duckstation_libretro_schema`; the live hotkey system uses `libretro_hotkey_defs`, not the adapter's `hotkeyBindingDefs()`.

## Changes

**Delete files:**
- `src/adapters/duckstation_adapter.cpp`
- `src/adapters/duckstation_adapter.h`
- `tests/test_format_binding.cpp`
- `tests/test_hotkey_defs.cpp`
- `tests/test_duckstation_schema.cpp`
- `tests/test_duckstation_controller_schema.cpp`

**`CMakeLists.txt`:**
- Remove the four dead test-target blocks (`add_executable` … `add_test`): `test_format_binding` (FormatBinding), `test_duckstation_schema` (DuckStationSchema), `test_duckstation_controller_schema` (DuckStationControllerSchema), `test_hotkey_defs` (HotkeyDefs).
- Remove every remaining `src/adapters/duckstation_adapter.cpp` source-list entry (6 in non-test targets, incl. the app) and the one `src/adapters/duckstation_adapter.h` header entry.

## Verification

1. `cmake --build cpp/build-x86_64 --target RetroNest` builds clean (catches any missed CMake reference / unexpected symbol dependency).
2. `ctest --test-dir cpp/build-x86_64` — remaining suite green; the four removed tests (FormatBinding, DuckStationSchema, DuckStationControllerSchema, HotkeyDefs) no longer registered.
3. `grep -rn "DuckStationAdapter\b\|duckstation_adapter" cpp/src cpp/tests cpp/CMakeLists.txt` (excluding `DuckStationLibretroAdapter`) returns nothing.

## Risk

Low — pure deletion. Sole failure mode is a missed CMake reference breaking the build, caught immediately by step 1. Not pushing until the user is satisfied (host `main`, direct-to-main per prior convention).

## Out of scope

Other standalone process adapters (dolphin/pcsx2/ppsspp/mgba) may be similarly runtime-dead, but this task is DuckStation only.
