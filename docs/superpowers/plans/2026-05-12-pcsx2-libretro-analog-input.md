# PCSX2 Libretro Analog Input (SP5.5) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the RetroNest-side plumbing for analog sticks (LeftX/Y, RightX/Y), L2/R2 analog triggers, rumble, player-2 port wiring, and a data-layer deadzone (radial for sticks, per-axis for triggers, no UI yet). Result: Ratchet & Clank 2 fully playable — right-stick camera, trigger strafe, rumble on weapon fire.

**Architecture:** Surgical extension of SP5's existing pattern. `InputRouter` keeps its lock-free `atomic<uint32_t>` digital bitmask AND gains a parallel `atomic<int16_t>` axis array per port (6 axes × 4 ports). The trampoline gains a `RETRO_DEVICE_ANALOG` branch alongside the existing `RETRO_DEVICE_JOYPAD` branch. `SdlInputManager` writes both digital (existing `+axis`/`-axis` emulation) and magnitude (new) on every `SDL_CONTROLLERAXISMOTION` event, using the existing `m_deviceIndices` map as the port. Rumble lives in a new `setRumbleMotor` method on `SdlInputManager` driven by a new `RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE` handler in `environment_callbacks.cpp`, bridged through `CoreRuntime` via a weak-bridge pattern that mirrors the existing NSView bridge.

**Tech Stack:** C++17, CMake, Qt6 (Core + Test), SDL2, libretro C ABI. RetroNest-side only — no upstream PCSX2 changes.

**Spec:** [`docs/superpowers/specs/2026-05-12-pcsx2-libretro-analog-input-design.md`](../specs/2026-05-12-pcsx2-libretro-analog-input-design.md)

**Testing model:** Hybrid: Task 1 is full TDD with Qt::Test unit tests for `InputRouter` (pure data structure, no Qt/SDL coupling); Tasks 2–5 are build-verified via `cmake --build`; Task 6 is the manual R&C 2 smoke test. This matches the SP3/SP5 split — pure logic gets unit tests, integration with SDL+PCSX2+real-hardware gets validated by the in-game smoke test.

**Working directory:** `/Users/mark/Documents/Projects/RetroNest-Project/`. The `Pcsx2 Experiment ` folder (note trailing space) is NOT touched in SP5.5 — PCSX2 side is already wired.

**Build command (every code task ends with):**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 2>&1 | tail -10
```

Expected: build succeeds. After Task 1, additionally:

```sh
ctest --test-dir /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64 --output-on-failure -R InputRouter
```

Expected: all `InputRouter` unit tests pass.

---

## File Structure

**Modified files (5):**

- `cpp/src/core/libretro/input_router.h` — add `RetroPadAxis` enum; `m_axes` array; `m_innerDeadzone` atomic; `setAxis`, `axis`, `setInnerDeadzone` API.
- `cpp/src/core/libretro/input_router.cpp` — implement new methods; radial deadzone math for stick pairs, per-axis for triggers; clamp deadzone to `[0.0, 0.5]`.
- `cpp/src/core/libretro/core_runtime.{h,cpp}` — add `m_sdlInput` member + `setSdlInputManager` setter; extend `inputStateTrampoline` with `RETRO_DEVICE_ANALOG` branch; add strong implementation of `coreRuntimeSetRumbleMotor` bridge; zero-rumble on `pause()` and in worker-loop teardown after `stop()`.
- `cpp/src/core/libretro/environment_callbacks.{h,cpp}` — add `RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE` (cmd 23) case + static `rumbleThunk`; weak-stub bridge `coreRuntimeSetRumbleMotor` (overridden by `core_runtime.cpp`'s strong def).
- `cpp/src/core/sdl_input_manager.{h,cpp}` — in `SDL_CONTROLLERAXISMOTION` branch, call `setAxis(devIdx, ...)` alongside existing digital writes; replace hardcoded `port=0` at the six button-write sites with `devIdx`; add `RumbleCache m_rumbleCache[NUM_PORTS]`; add `kRumbleDurationMs = 100`; add public `bool setRumbleMotor(int port, retro_rumble_effect motor, uint16_t strength)`.
- `cpp/src/core/game_session.cpp` — at the two existing `setEmulationMode` call sites (`:355`, `:409`), also call `rt->setSdlInputManager(m_sdlInputManager)`.
- `cpp/tests/test_input_router.cpp` — extend with seven new test slots (axis storage, deadzone, radial, port isolation).

**No changes to:** `audio_sink.{h,cpp}`, `core_loader.{h,cpp}`, `video_software.{h,cpp}`, `options_store.{h,cpp}`, `rcheevos_runtime.{h,cpp}`, `cpp/CMakeLists.txt` (the existing `test_input_router` target already links `input_router.cpp`, and no new files are added).

**No upstream PCSX2 changes.** SP5 already wrote the analog half-axis bindings, `DualShock2` type, and L2/R2 analog binding. PCSX2's default deadzone is `0.0f` — RetroNest-side deadzone is conflict-free.

---

## Task 1 — Extend `InputRouter` with analog axes + radial deadzone (TDD)

Adds the data-structure surface that the rest of the plan reads from and writes to. Fully unit-testable in isolation — no Qt/SDL/PCSX2 dependency. Done first because every later task depends on it.

**Files:**

- Modify: `cpp/src/core/libretro/input_router.h:1-72` (whole file)
- Modify: `cpp/src/core/libretro/input_router.cpp:1-27` (whole file)
- Modify: `cpp/tests/test_input_router.cpp:1-37` (extend existing TestInputRouter class)

### Step 1.1: Write the failing tests

Replace `cpp/tests/test_input_router.cpp` entirely with:

```cpp
#include <QtTest>
#include "core/libretro/input_router.h"

class TestInputRouter : public QObject {
    Q_OBJECT
private slots:
    // --- existing digital tests (unchanged) ---
    void testInitialStateIsZero() {
        InputRouter r;
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::A));
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::Up));
    }
    void testSetAndReadButton() {
        InputRouter r;
        r.setButtonPressed(0, RetroPadSlot::A, true);
        QVERIFY(r.buttonPressed(0, RetroPadSlot::A));
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::B));
        r.setButtonPressed(0, RetroPadSlot::A, false);
        QVERIFY(!r.buttonPressed(0, RetroPadSlot::A));
    }
    void testBindingLookup() {
        InputRouter r;
        r.bind(0, "A", RetroPadSlot::A);
        r.bind(0, "DPadUp", RetroPadSlot::Up);
        QCOMPARE(r.lookup(0, "A"), RetroPadSlot::A);
        QCOMPARE(r.lookup(0, "DPadUp"), RetroPadSlot::Up);
        QCOMPARE(r.lookup(0, "Unknown"), RetroPadSlot::None);
    }
    void testPortsAreIndependent() {
        InputRouter r;
        r.setButtonPressed(0, RetroPadSlot::A, true);
        QVERIFY(r.buttonPressed(0, RetroPadSlot::A));
        QVERIFY(!r.buttonPressed(1, RetroPadSlot::A));
    }

    // --- new analog tests (SP5.5) ---

    void testInitialAxesAreZero() {
        InputRouter r;
        QCOMPARE(r.axis(0, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::LeftY), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::RightX), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::RightY), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::R2), int16_t(0));
    }

    void testTriggerPerAxisDeadzoneAndPassthrough() {
        InputRouter r;
        // Default deadzone 0.15 → threshold ≈ 4915 of 32767.
        r.setAxis(0, RetroPadAxis::L2, 3000);   // below threshold
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(0));
        r.setAxis(0, RetroPadAxis::L2, 16000);  // well above
        const int16_t v = r.axis(0, RetroPadAxis::L2);
        QVERIFY(v > 0);
        QVERIFY(v < 16000);   // rescaled by deadzone
    }

    void testStickRadialDeadzoneZeroesBothBelow() {
        InputRouter r;
        // X=3000, Y=3000 → magnitude ≈ 4243 < 4915 deadzone → both should be zero.
        r.setAxis(0, RetroPadAxis::LeftX, 3000);
        r.setAxis(0, RetroPadAxis::LeftY, 3000);
        QCOMPARE(r.axis(0, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(0, RetroPadAxis::LeftY), int16_t(0));
    }

    void testStickRadialDeadzoneReleasesBothAbove() {
        InputRouter r;
        // X=4000, Y=4000 → magnitude ≈ 5657 > 4915 → both should be non-zero.
        r.setAxis(0, RetroPadAxis::LeftX, 4000);
        r.setAxis(0, RetroPadAxis::LeftY, 4000);
        QVERIFY(r.axis(0, RetroPadAxis::LeftX) > 0);
        QVERIFY(r.axis(0, RetroPadAxis::LeftY) > 0);
    }

    void testStickRadialPreservesSign() {
        InputRouter r;
        r.setAxis(0, RetroPadAxis::RightX, -20000);
        r.setAxis(0, RetroPadAxis::RightY,  20000);
        QVERIFY(r.axis(0, RetroPadAxis::RightX) < 0);
        QVERIFY(r.axis(0, RetroPadAxis::RightY) > 0);
    }

    void testDeadzoneZeroIsPassthrough() {
        InputRouter r;
        r.setInnerDeadzone(0.0f);
        r.setAxis(0, RetroPadAxis::L2, 1);
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(1));
        r.setAxis(0, RetroPadAxis::LeftX, 12345);
        r.setAxis(0, RetroPadAxis::LeftY, 0);
        QCOMPARE(r.axis(0, RetroPadAxis::LeftX), int16_t(12345));
    }

    void testDeadzoneClampedToHalf() {
        InputRouter r;
        r.setInnerDeadzone(0.6f);   // should clamp to 0.5
        // At dz=0.5, threshold = 16383. A value of 16000 should still register 0.
        r.setAxis(0, RetroPadAxis::L2, 16000);
        QCOMPARE(r.axis(0, RetroPadAxis::L2), int16_t(0));
        // A value of 20000 (above 16383) should register non-zero.
        r.setAxis(0, RetroPadAxis::L2, 20000);
        QVERIFY(r.axis(0, RetroPadAxis::L2) > 0);
    }

    void testAxisPortIsolation() {
        InputRouter r;
        r.setAxis(0, RetroPadAxis::LeftX, 30000);
        QCOMPARE(r.axis(1, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(2, RetroPadAxis::LeftX), int16_t(0));
        QCOMPARE(r.axis(3, RetroPadAxis::LeftX), int16_t(0));
    }
};
QTEST_MAIN(TestInputRouter)
#include "test_input_router.moc"
```

### Step 1.2: Build and run — confirm the new tests fail to compile

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 --target test_input_router 2>&1 | tail -20
```

Expected: compile error like `'RetroPadAxis' is not a member of namespace 'InputRouter'` (or similar) — the symbols don't exist yet.

### Step 1.3: Add the API surface to `input_router.h`

Replace `cpp/src/core/libretro/input_router.h` entirely with:

```cpp
#pragma once
#include <QHash>
#include <QString>
#include <array>
#include <atomic>
#include <QtGlobal>

enum class RetroPadSlot : int {
    None   = -1,
    B      = 0,
    Y      = 1,
    Select = 2,
    Start  = 3,
    Up     = 4,
    Down   = 5,
    Left   = 6,
    Right  = 7,
    A      = 8,
    X      = 9,
    L      = 10,
    R      = 11,
    L2     = 12,
    R2     = 13,
    L3     = 14,
    R3     = 15,
    Count  = 16
};

/**
 * Six analog axes per port. Sticks are paired (LeftX/Y, RightX/Y) and use a
 * radial deadzone (vector magnitude check). Triggers (L2, R2) are 1D and use
 * a per-axis deadzone.
 */
enum class RetroPadAxis : int {
    LeftX  = 0,
    LeftY  = 1,
    RightX = 2,
    RightY = 3,
    L2     = 4,
    R2     = 5,
    Count  = 6
};

/**
 * Map a binding action key string (as stored in controls.ini) to the
 * corresponding RetroPadSlot enum value.  Returns RetroPadSlot::None for
 * unrecognised keys so callers can skip them silently.
 */
inline RetroPadSlot retroPadSlotFromKey(const QString& key) {
    if (key == QStringLiteral("B"))      return RetroPadSlot::B;
    if (key == QStringLiteral("Y"))      return RetroPadSlot::Y;
    if (key == QStringLiteral("Select")) return RetroPadSlot::Select;
    if (key == QStringLiteral("Start"))  return RetroPadSlot::Start;
    if (key == QStringLiteral("Up"))     return RetroPadSlot::Up;
    if (key == QStringLiteral("Down"))   return RetroPadSlot::Down;
    if (key == QStringLiteral("Left"))   return RetroPadSlot::Left;
    if (key == QStringLiteral("Right"))  return RetroPadSlot::Right;
    if (key == QStringLiteral("A"))      return RetroPadSlot::A;
    if (key == QStringLiteral("X"))      return RetroPadSlot::X;
    if (key == QStringLiteral("L"))      return RetroPadSlot::L;
    if (key == QStringLiteral("R"))      return RetroPadSlot::R;
    if (key == QStringLiteral("L2"))     return RetroPadSlot::L2;
    if (key == QStringLiteral("R2"))     return RetroPadSlot::R2;
    if (key == QStringLiteral("L3"))     return RetroPadSlot::L3;
    if (key == QStringLiteral("R3"))     return RetroPadSlot::R3;
    return RetroPadSlot::None;
}

class InputRouter {
public:
    static constexpr int NUM_PORTS = 4;
    static constexpr int NUM_AXES_PER_PORT = static_cast<int>(RetroPadAxis::Count);

    /** Bind: (device index, canonical SDL element name) -> RetroPad slot. */
    void bind(int deviceIdx, const QString& sdlElement, RetroPadSlot slot);
    void clearBindings();

    /** Lookup: returns RetroPadSlot::None if unbound. */
    RetroPadSlot lookup(int deviceIdx, const QString& sdlElement) const;

    void setButtonPressed(int port, RetroPadSlot slot, bool down);
    bool buttonPressed(int port, RetroPadSlot slot) const;

    /**
     * Write raw SDL axis value (-32768..32767) to storage. Called from the
     * Qt/SDL thread. Lock-free atomic store.
     */
    void setAxis(int port, RetroPadAxis axis, int16_t raw);

    /**
     * Read axis value with deadzone applied. Called from the core thread.
     * For LeftX/Y and RightX/Y, applies a radial (vector-magnitude)
     * deadzone using the paired axis. For L2/R2, applies a 1D per-axis
     * deadzone. Returns 0 inside the deadzone, sign-preserving rescaled
     * value outside.
     */
    int16_t axis(int port, RetroPadAxis axis) const;

    /**
     * Set the inner deadzone as a fraction of full scale (0.0..0.5).
     * Default 0.15. Values outside [0.0, 0.5] are clamped. Future RetroNest
     * settings UI will bind here; SP5.5 leaves it at default.
     */
    void setInnerDeadzone(float fraction);

private:
    QHash<QPair<int, QString>, RetroPadSlot> m_bindings;
    std::array<std::atomic<uint32_t>, NUM_PORTS> m_state{};
    std::array<std::atomic<int16_t>, NUM_PORTS * NUM_AXES_PER_PORT> m_axes{};
    std::atomic<float> m_innerDeadzone{0.15f};
};
```

### Step 1.4: Implement the methods in `input_router.cpp`

Replace `cpp/src/core/libretro/input_router.cpp` entirely with:

```cpp
#include "input_router.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

void InputRouter::bind(int deviceIdx, const QString& sdlElement, RetroPadSlot slot) {
    m_bindings[{deviceIdx, sdlElement}] = slot;
}

void InputRouter::clearBindings() { m_bindings.clear(); }

RetroPadSlot InputRouter::lookup(int deviceIdx, const QString& sdlElement) const {
    auto it = m_bindings.constFind({deviceIdx, sdlElement});
    return (it == m_bindings.constEnd()) ? RetroPadSlot::None : it.value();
}

void InputRouter::setButtonPressed(int port, RetroPadSlot slot, bool down) {
    if (port < 0 || port >= NUM_PORTS || slot == RetroPadSlot::None) return;
    uint32_t bit = 1u << static_cast<int>(slot);
    auto& s = m_state[port];
    if (down) s.fetch_or(bit, std::memory_order_relaxed);
    else      s.fetch_and(~bit, std::memory_order_relaxed);
}

bool InputRouter::buttonPressed(int port, RetroPadSlot slot) const {
    if (port < 0 || port >= NUM_PORTS || slot == RetroPadSlot::None) return false;
    uint32_t bit = 1u << static_cast<int>(slot);
    return (m_state[port].load(std::memory_order_relaxed) & bit) != 0;
}

void InputRouter::setAxis(int port, RetroPadAxis axis, int16_t raw) {
    if (port < 0 || port >= NUM_PORTS) return;
    const int idx = static_cast<int>(axis);
    if (idx < 0 || idx >= NUM_AXES_PER_PORT) return;
    m_axes[port * NUM_AXES_PER_PORT + idx].store(raw, std::memory_order_relaxed);
}

void InputRouter::setInnerDeadzone(float fraction) {
    fraction = std::clamp(fraction, 0.0f, 0.5f);
    m_innerDeadzone.store(fraction, std::memory_order_relaxed);
}

int16_t InputRouter::axis(int port, RetroPadAxis axis) const {
    if (port < 0 || port >= NUM_PORTS) return 0;
    const int idx = static_cast<int>(axis);
    if (idx < 0 || idx >= NUM_AXES_PER_PORT) return 0;

    const int16_t raw =
        m_axes[port * NUM_AXES_PER_PORT + idx].load(std::memory_order_relaxed);
    const float dzFrac = m_innerDeadzone.load(std::memory_order_relaxed);
    const float dz = dzFrac * 32767.0f;

    // Triggers: per-axis 1D deadzone.
    if (axis == RetroPadAxis::L2 || axis == RetroPadAxis::R2) {
        const float fabsRaw = std::fabs(static_cast<float>(raw));
        if (fabsRaw < dz) return 0;
        // Sign-preserving rescale: (|raw| - dz) / (32767 - dz) * 32767.
        const float denom = 32767.0f - dz;
        if (denom <= 0.0f) return raw;  // safety: dz clamp prevents this in practice
        const float scaled = (fabsRaw - dz) / denom * 32767.0f;
        return static_cast<int16_t>(raw < 0 ? -scaled : scaled);
    }

    // Sticks: radial deadzone using the paired axis.
    RetroPadAxis pair;
    switch (axis) {
        case RetroPadAxis::LeftX:  pair = RetroPadAxis::LeftY;  break;
        case RetroPadAxis::LeftY:  pair = RetroPadAxis::LeftX;  break;
        case RetroPadAxis::RightX: pair = RetroPadAxis::RightY; break;
        case RetroPadAxis::RightY: pair = RetroPadAxis::RightX; break;
        default: return 0;
    }
    const int16_t other =
        m_axes[port * NUM_AXES_PER_PORT + static_cast<int>(pair)]
            .load(std::memory_order_relaxed);

    const float rawF   = static_cast<float>(raw);
    const float otherF = static_cast<float>(other);
    const float mag = std::sqrt(rawF * rawF + otherF * otherF);
    if (mag < dz) return 0;

    const float denom = 32767.0f - dz;
    if (denom <= 0.0f) return raw;
    const float scaledMag = (mag - dz) / denom * 32767.0f;
    // Scale this component proportionally: raw * (scaledMag / mag).
    const float comp = rawF * (scaledMag / mag);
    // Clamp to int16 range (just in case of fp rounding past 32767).
    if (comp >  32767.0f) return  32767;
    if (comp < -32768.0f) return -32768;
    return static_cast<int16_t>(comp);
}
```

### Step 1.5: Build and run tests — confirm green

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 --target test_input_router 2>&1 | tail -10
```

Expected: build succeeds.

Then:

```sh
ctest --test-dir /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64 --output-on-failure -R InputRouter
```

Expected:

```
1/1 Test #N: InputRouter ......................... Passed
100% tests passed, 0 tests failed out of 1
```

### Step 1.6: Full build sanity check

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 2>&1 | tail -10
```

Expected: full RetroNest build still succeeds — confirms no other consumer of `InputRouter` was broken (every other site uses only `setButtonPressed`/`buttonPressed`/`bind`/`lookup`, unchanged).

### Step 1.7: Commit

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/input_router.h \
        cpp/src/core/libretro/input_router.cpp \
        cpp/tests/test_input_router.cpp
git commit -m "feat(input): add analog axis storage + radial deadzone to InputRouter

Adds RetroPadAxis enum and a per-port int16 axis array alongside the
existing digital bitmask. axis() reader applies radial deadzone for
stick pairs and per-axis deadzone for L2/R2 triggers. Deadzone is a
data-layer atomic<float> defaulting to 0.15; clamped to [0.0, 0.5].

Lock-free atomic per axis preserves the SP5 threading model: writes
from the Qt thread, reads from the core thread, no mutex.

Unit-tested in test_input_router.cpp covering zero initial state,
trigger passthrough, radial zero/release symmetry, sign preservation,
deadzone clamp, and port isolation."
```

---

## Task 2 — Extend `inputStateTrampoline` with the `RETRO_DEVICE_ANALOG` branch

Wires PCSX2's analog queries to the new `InputRouter::axis()` reader. After this task, the read path is complete: any value already in `m_axes` flows out to PCSX2. (Writes still come in only via the existing digital `+axis`/`-axis` path; magnitude wiring comes in Task 3.)

**Files:**

- Modify: `cpp/src/core/libretro/core_runtime.cpp:99-108` (`inputStateTrampoline`)

### Step 2.1: Replace the trampoline body

In `cpp/src/core/libretro/core_runtime.cpp`, replace lines 99–108 (the entire `inputStateTrampoline` function):

```cpp
int16_t CoreRuntime::inputStateTrampoline(unsigned port, unsigned device,
                                          unsigned /*index*/, unsigned id) {
    // NOTE: InputRouter::lookup() is NOT used from the core thread; lookups
    // happen on the Qt thread (via SdlInputManager) which then writes the
    // atomic bitmask via setButtonPressed(). We only call buttonPressed() here,
    // which reads an atomic<uint32_t> — safe across threads without a lock.
    if (!g_current || device != RETRO_DEVICE_JOYPAD) return 0;
    auto slot = static_cast<RetroPadSlot>(id);
    return g_current->m_input.buttonPressed(static_cast<int>(port), slot) ? 1 : 0;
}
```

with:

```cpp
int16_t CoreRuntime::inputStateTrampoline(unsigned port, unsigned device,
                                          unsigned index, unsigned id) {
    // NOTE: InputRouter::lookup() is NOT used from the core thread; lookups
    // happen on the Qt thread (via SdlInputManager) which then writes the
    // atomic state. We only call buttonPressed() / axis() here, both of which
    // read atomics — safe across threads without a lock.
    if (!g_current) return 0;

    if (device == RETRO_DEVICE_JOYPAD) {
        auto slot = static_cast<RetroPadSlot>(id);
        return g_current->m_input.buttonPressed(static_cast<int>(port), slot) ? 1 : 0;
    }

    if (device == RETRO_DEVICE_ANALOG) {
        // Map (index, id) → RetroPadAxis. Unknown combinations return 0
        // (forward-compat with future libretro spec extensions).
        RetroPadAxis a = RetroPadAxis::Count;
        if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT) {
            if      (id == RETRO_DEVICE_ID_ANALOG_X) a = RetroPadAxis::LeftX;
            else if (id == RETRO_DEVICE_ID_ANALOG_Y) a = RetroPadAxis::LeftY;
        } else if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) {
            if      (id == RETRO_DEVICE_ID_ANALOG_X) a = RetroPadAxis::RightX;
            else if (id == RETRO_DEVICE_ID_ANALOG_Y) a = RetroPadAxis::RightY;
        } else if (index == RETRO_DEVICE_INDEX_ANALOG_BUTTON) {
            if      (id == RETRO_DEVICE_ID_JOYPAD_L2) a = RetroPadAxis::L2;
            else if (id == RETRO_DEVICE_ID_JOYPAD_R2) a = RetroPadAxis::R2;
        }
        if (a == RetroPadAxis::Count) return 0;
        return g_current->m_input.axis(static_cast<int>(port), a);
    }

    return 0;
}
```

### Step 2.2: Build sanity check

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 2>&1 | tail -10
```

Expected: build succeeds. The libretro constants (`RETRO_DEVICE_ANALOG`, `RETRO_DEVICE_INDEX_ANALOG_*`, `RETRO_DEVICE_ID_ANALOG_*`, `RETRO_DEVICE_ID_JOYPAD_L2/R2`) are all already in `vendor/libretro-api/libretro.h` and `core_runtime.cpp` already includes `libretro.h` transitively via `core_loader.h`.

### Step 2.3: Commit

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/core_runtime.cpp
git commit -m "feat(input): handle RETRO_DEVICE_ANALOG in input trampoline

PCSX2's LibretroInputSource queries RETRO_DEVICE_ANALOG for stick X/Y
on LEFT/RIGHT and analog L2/R2 on BUTTON. Map (index, id) -> RetroPadAxis
and forward to InputRouter::axis(). Unknown index/id combinations under
ANALOG return 0 for forward-compat. JOYPAD path unchanged."
```

---

## Task 3 — `SdlInputManager`: write axis magnitudes + fix port for digital writes

Closes the write path: every `SDL_CONTROLLERAXISMOTION` event also stores magnitude in `InputRouter`. Also fixes the SP5 hardcode where port-2 button presses leaked into port 0 — replaces all six `setButtonPressed(0, ...)` call sites with `setButtonPressed(devIdx, ...)` and uses the same `devIdx` for the new `setAxis` call.

After this task, sticks + triggers should work end-to-end. R&C 2 smoke-test step 1–4 from Task 6 can be exercised here as a sanity check, before rumble (Task 4) is added.

**Files:**

- Modify: `cpp/src/core/sdl_input_manager.cpp:434, :509, :523-565` (one new helper function, axis-motion branch additions, port-arg replacements)

### Step 3.1: Add the `sdlAxisToRetroPadAxis` helper

In `cpp/src/core/sdl_input_manager.cpp`, add this static helper near the top of the file, right after the existing `canonicalName` static map (search for `canonicalName(const char* sdlName)` to find the right area; add immediately after the function body):

```cpp
// Map SDL_GameControllerAxis -> RetroPadAxis. Returns RetroPadAxis::Count
// for SDL axes we don't surface as analog (currently none — all 6 SDL axes
// map onto our enum).
static RetroPadAxis sdlAxisToRetroPadAxis(SDL_GameControllerAxis a) {
    switch (a) {
        case SDL_CONTROLLER_AXIS_LEFTX:        return RetroPadAxis::LeftX;
        case SDL_CONTROLLER_AXIS_LEFTY:        return RetroPadAxis::LeftY;
        case SDL_CONTROLLER_AXIS_RIGHTX:       return RetroPadAxis::RightX;
        case SDL_CONTROLLER_AXIS_RIGHTY:       return RetroPadAxis::RightY;
        case SDL_CONTROLLER_AXIS_TRIGGERLEFT:  return RetroPadAxis::L2;
        case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return RetroPadAxis::R2;
        default:                               return RetroPadAxis::Count;
    }
}
```

### Step 3.2: Fix the button-write port at the digital-write call sites

In `cpp/src/core/sdl_input_manager.cpp`:

At line 434 (inside `SDL_CONTROLLERBUTTONDOWN` → emulation branch), change:

```cpp
m_emulationTarget->setButtonPressed(0, slot, true);
```

to:

```cpp
m_emulationTarget->setButtonPressed(devIdx, slot, true);
```

At line 509 (inside `SDL_CONTROLLERBUTTONUP` → emulation branch), change:

```cpp
m_emulationTarget->setButtonPressed(0, slot, false);
```

to:

```cpp
m_emulationTarget->setButtonPressed(devIdx, slot, false);
```

### Step 3.3: Fix port + add `setAxis` writes in the axis-motion branch

In `cpp/src/core/sdl_input_manager.cpp`, replace the entire `else if (m_emulationTarget && !m_capturing)` block inside `case SDL_CONTROLLERAXISMOTION:` (currently at lines 536–565):

```cpp
            } else if (m_emulationTarget && !m_capturing) {
                // Route axis polarity to the InputRouter so analog-mapped-to-
                // RetroPad bindings work (e.g. D-Pad Up = SDL-0/-LeftY).
                // Resolve both "+{axis}" and "-{axis}" via lookup; only the
                // bound polarity gets a press, the opposite gets released.
                const char* axisName = SDL_GameControllerGetStringForAxis(
                    static_cast<SDL_GameControllerAxis>(event.caxis.axis));
                const QString axis = canonicalName(axisName);
                const QString posEl = "+" + axis;
                const QString negEl = "-" + axis;
                const int devIdx = m_deviceIndices.value(event.caxis.which, 0);
                const auto posSlot = m_emulationTarget->lookup(devIdx, posEl);
                const auto negSlot = m_emulationTarget->lookup(devIdx, negEl);

                if (value > kAxisDeadzone) {
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, posSlot, true);
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, negSlot, false);
                } else if (value < -kAxisDeadzone) {
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, negSlot, true);
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, posSlot, false);
                } else if (std::abs(value) <= kAxisDeadzone / 2) {
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, posSlot, false);
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(0, negSlot, false);
                }
            }
```

with:

```cpp
            } else if (m_emulationTarget && !m_capturing) {
                // Two-fold routing on every axis event:
                //  1. Existing: '+axis'/'-axis' bindings write digital presses
                //     into the InputRouter bitmask. Keeps D-Pad-bound-to-stick
                //     and other digital-emulation bindings working.
                //  2. SP5.5: write the raw int16 magnitude into the axis
                //     storage. PCSX2's analog bindings ("-LeftY", "+L2", ...)
                //     query this via RETRO_DEVICE_ANALOG.
                const auto sdlAxis = static_cast<SDL_GameControllerAxis>(event.caxis.axis);
                const char* axisName = SDL_GameControllerGetStringForAxis(sdlAxis);
                const QString axis = canonicalName(axisName);
                const QString posEl = "+" + axis;
                const QString negEl = "-" + axis;
                const int devIdx = m_deviceIndices.value(event.caxis.which, 0);
                const auto posSlot = m_emulationTarget->lookup(devIdx, posEl);
                const auto negSlot = m_emulationTarget->lookup(devIdx, negEl);

                // (1) Digital emulation, unchanged in behaviour; port fixed.
                if (value > kAxisDeadzone) {
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(devIdx, posSlot, true);
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(devIdx, negSlot, false);
                } else if (value < -kAxisDeadzone) {
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(devIdx, negSlot, true);
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(devIdx, posSlot, false);
                } else if (std::abs(value) <= kAxisDeadzone / 2) {
                    if (posSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(devIdx, posSlot, false);
                    if (negSlot != RetroPadSlot::None)
                        m_emulationTarget->setButtonPressed(devIdx, negSlot, false);
                }

                // (2) Analog magnitude — raw int16, deadzone applied at read time.
                const RetroPadAxis rpAxis = sdlAxisToRetroPadAxis(sdlAxis);
                if (rpAxis != RetroPadAxis::Count) {
                    m_emulationTarget->setAxis(devIdx, rpAxis,
                                               static_cast<int16_t>(value));
                }
            }
```

### Step 3.4: Build sanity check

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 2>&1 | tail -10
```

Expected: build succeeds.

### Step 3.5: Existing-test regression check

Run:

```sh
ctest --test-dir /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64 --output-on-failure 2>&1 | tail -20
```

Expected: all tests still pass (no test for `SdlInputManager`; this confirms no compile/link regression).

### Step 3.6: Optional interim smoke test

If you have R&C 2 ready and want to gate Task 4 (rumble), launch a build via the RetroNest UI and confirm:

- Main-menu Controller test screen shows live left + right stick + L2/R2 values.
- In-game right stick rotates camera.
- L2/R2 strafe.
- Left stick walks Ratchet in any direction.

Rumble will NOT fire yet — that's Task 4.

If smoke is fine, proceed. If not, debug before Task 4 (use the trace gate added in Task 5 if needed, or add `qDebug` statements temporarily).

### Step 3.7: Commit

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/sdl_input_manager.cpp
git commit -m "feat(input): write analog magnitudes + fix port for digital writes

On every SDL_CONTROLLERAXISMOTION event, in addition to the existing
'+axis'/'-axis' digital-emulation writes, store the raw int16 magnitude
in InputRouter::setAxis. PCSX2's LibretroInputSource queries these via
RETRO_DEVICE_ANALOG.

Also fix an SP5-era hardcode where setButtonPressed was always called
with port=0, which broke player 2 input. Use the existing devIdx (from
m_deviceIndices) as the port at all six emulation-mode write sites.
First-opened controller = port 0, second = port 1, etc."
```

---

## Task 4 — Rumble end-to-end

Adds rumble across five files (`environment_callbacks`, `core_runtime`, `sdl_input_manager`, `game_session`, plus a small constant). Uses the same weak-bridge pattern as the existing `coreRuntimeGetActiveNSView` so `environment_callbacks.cpp` stays decoupled from `CoreRuntime`'s headers.

**Files:**

- Modify: `cpp/src/core/libretro/environment_callbacks.h` (declare bridge)
- Modify: `cpp/src/core/libretro/environment_callbacks.cpp` (env handler + thunk + weak bridge stub)
- Modify: `cpp/src/core/libretro/core_runtime.h` (forward-declare SdlInputManager, add member + setter)
- Modify: `cpp/src/core/libretro/core_runtime.cpp` (strong bridge, pause/stop zero-rumble)
- Modify: `cpp/src/core/sdl_input_manager.h` (declare `setRumbleMotor`, `RumbleCache`, `kRumbleDurationMs`)
- Modify: `cpp/src/core/sdl_input_manager.cpp` (implement `setRumbleMotor`, clear cache on `closeController`)
- Modify: `cpp/src/core/game_session.cpp` (wire `setSdlInputManager` at the two `setEmulationMode` sites)

### Step 4.1: Add `SdlInputManager::setRumbleMotor` declaration + cache fields

In `cpp/src/core/sdl_input_manager.h`, add `#include "libretro.h"` near the top alongside the existing `#include "core/libretro/input_router.h"`:

```cpp
#include "core/libretro/input_router.h"
#include "libretro.h"   // retro_rumble_effect
```

Add this constant inside the class, in a public section (just above `setEmulationMode` is a good spot):

```cpp
    static constexpr uint32_t kRumbleDurationMs = 100;

    /**
     * Fire one motor on the controller mapped to `port`. PCSX2's rumble
     * interface delivers STRONG and WEAK as separate per-motor calls; we
     * cache the last value of each and merge before invoking SDL so both
     * motors stay alive when the core only updates one.
     *
     * Returns false if no controller is currently mapped to `port` (e.g.
     * disconnected mid-game) — the libretro contract has no failure path
     * for set_rumble_state, so the caller can ignore the return.
     */
    bool setRumbleMotor(int port, retro_rumble_effect motor, uint16_t strength);
```

Add this private struct + field just above `m_emulationTarget` in the private section:

```cpp
    struct RumbleCache {
        std::atomic<uint16_t> low{0};   // RETRO_RUMBLE_STRONG
        std::atomic<uint16_t> high{0};  // RETRO_RUMBLE_WEAK
    };
    RumbleCache m_rumbleCache[InputRouter::NUM_PORTS];
```

Add `#include <atomic>` near the existing standard includes if not already present (it is via Qt headers, but be explicit).

### Step 4.2: Implement `setRumbleMotor` in `sdl_input_manager.cpp`

Add at the end of `cpp/src/core/sdl_input_manager.cpp` (after `clearEmulationMode`):

```cpp
bool SdlInputManager::setRumbleMotor(int port, retro_rumble_effect motor,
                                     uint16_t strength) {
    if (port < 0 || port >= InputRouter::NUM_PORTS) return false;

    if (motor == RETRO_RUMBLE_STRONG)
        m_rumbleCache[port].low.store(strength, std::memory_order_relaxed);
    else if (motor == RETRO_RUMBLE_WEAK)
        m_rumbleCache[port].high.store(strength, std::memory_order_relaxed);
    else
        return false;

    // Reverse-lookup the SDL_JoystickID whose deviceIndex == port.
    SDL_JoystickID jid = -1;
    for (auto it = m_deviceIndices.constBegin(); it != m_deviceIndices.constEnd(); ++it) {
        if (it.value() == port) { jid = it.key(); break; }
    }
    if (jid < 0) return false;

    SDL_GameController* ctrl = m_controllers.value(jid, nullptr);
    if (!ctrl) return false;

    const uint16_t low  = m_rumbleCache[port].low.load(std::memory_order_relaxed);
    const uint16_t high = m_rumbleCache[port].high.load(std::memory_order_relaxed);
    SDL_GameControllerRumble(ctrl, low, high, kRumbleDurationMs);
    return true;
}
```

Also: in `closeController` (`sdl_input_manager.cpp:374-383`), after the existing `m_controllerTypes.remove(instanceId);`, add a zero-then-clear for that port's cache. Replace the body of `closeController` with:

```cpp
void SdlInputManager::closeController(SDL_JoystickID instanceId) {
    if (auto* ctrl = m_controllers.value(instanceId, nullptr)) {
        qInfo() << "[SDL] Controller disconnected:" << SDL_GameControllerName(ctrl);
        // If this controller had rumble active, stop it before closing the
        // SDL handle — otherwise the motors keep running until SDL's
        // duration expires (or the OS scrubs them on close, which is
        // platform-dependent).
        const int port = m_deviceIndices.value(instanceId, -1);
        if (port >= 0 && port < InputRouter::NUM_PORTS) {
            SDL_GameControllerRumble(ctrl, 0, 0, 0);
            m_rumbleCache[port].low.store(0, std::memory_order_relaxed);
            m_rumbleCache[port].high.store(0, std::memory_order_relaxed);
        }
        SDL_GameControllerClose(ctrl);
        m_controllers.remove(instanceId);
        m_deviceIndices.remove(instanceId);
        m_controllerTypes.remove(instanceId);
        emit controllersChanged();
    }
}
```

### Step 4.3: Build sanity check after sdl_input_manager changes

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 2>&1 | tail -10
```

Expected: builds succeed. No callers of `setRumbleMotor` yet — that's next.

### Step 4.4: Add `coreRuntimeSetRumbleMotor` bridge declaration

In `cpp/src/core/libretro/environment_callbacks.h`, just after the existing `coreRuntimeGetActiveNSView` declaration (lines 51-52 region):

```cpp
/**
 * Bridge function to fire rumble on the SdlInputManager that the active
 * CoreRuntime is wired to. Implemented in core_runtime.cpp; used by
 * environment_callbacks for the RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE
 * thunk. Weak stub in environment_callbacks.cpp lets test_environment_callbacks
 * link without dragging in core_runtime.
 *
 * Returns true if the call reached a live controller, false otherwise.
 * The libretro set_rumble_state contract has no failure semantics, so
 * callers should ignore the return value in production.
 */
extern "C" bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                          unsigned port,
                                          unsigned effect,
                                          uint16_t strength);
```

Also add an include of `<cstdint>` near the top if not already present:

```cpp
#include <cstdint>
```

### Step 4.5: Add the env handler + weak bridge stub in `environment_callbacks.cpp`

In `cpp/src/core/libretro/environment_callbacks.cpp`, just after the existing weak `coreRuntimeGetActiveNSView` stub (lines 47-53 region), add the weak `coreRuntimeSetRumbleMotor` stub plus the static thunk:

```cpp
// Weak stub for the rumble bridge — strong override in core_runtime.cpp.
extern "C" bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                          unsigned port,
                                          unsigned effect,
                                          uint16_t strength) __attribute__((weak));
bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                unsigned port,
                                unsigned effect,
                                uint16_t strength) {
    (void)runtime_opaque; (void)port; (void)effect; (void)strength;
    return false;
}

// Static thunk the core stores via retro_rumble_interface.set_rumble_state.
// Looks up the active CoreRuntime through the same g_current pattern used
// by the NSView bridge — but indirectly, via the EnvironmentContext that
// the dispatch already has access to. Because libretro doesn't pass our
// context pointer to the thunk, we route through a file-scope pointer
// stashed at GET_RUMBLE_INTERFACE time.
namespace {
EnvironmentContext* g_rumbleCtx = nullptr;
bool rumbleThunk(unsigned port, retro_rumble_effect effect, uint16_t strength) {
    if (!g_rumbleCtx || !g_rumbleCtx->runtime) return false;
    return coreRuntimeSetRumbleMotor(g_rumbleCtx->runtime, port,
                                     static_cast<unsigned>(effect), strength);
}
}
```

Then in `environmentDispatch` (large `switch` in the same file), add a new `case` near the existing GET-style handlers (place it next to `RETRO_ENVIRONMENT_GET_VARIABLE` and similar — e.g. just before the `case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:` block, line 96 area):

```cpp
        case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE: {
            auto* iface = static_cast<retro_rumble_interface*>(data);
            if (!iface) return false;
            g_rumbleCtx = ctx;             // stash for the thunk
            iface->set_rumble_state = &rumbleThunk;
            return true;
        }
```

### Step 4.6: Add `m_sdlInput` member + setter to `CoreRuntime`

In `cpp/src/core/libretro/core_runtime.h`, add a forward declaration above the class (or `#include "core/sdl_input_manager.h"` if you prefer — forward-decl avoids an include cycle; CoreRuntime only stores a pointer):

```cpp
class SdlInputManager;
```

Add the public setter, just below the existing `InputRouter& input() { return m_input; }` accessor (line 98 region):

```cpp
    /**
     * Register the SdlInputManager that backs this runtime's input. The
     * runtime does not own the pointer; the caller (GameSession) ensures
     * the manager outlives the runtime. Used by the rumble bridge to
     * route libretro's set_rumble_state calls to SDL_GameControllerRumble.
     * Pass nullptr to clear.
     */
    void setSdlInputManager(SdlInputManager* sdl) { m_sdlInput = sdl; }
    SdlInputManager* sdlInputManager() const { return m_sdlInput; }
```

Add the private member, near the other non-owning pointers (top of the private section):

```cpp
    SdlInputManager* m_sdlInput = nullptr;
```

### Step 4.7: Strong-define the bridge in `core_runtime.cpp`

In `cpp/src/core/libretro/core_runtime.cpp`, add an include near the top:

```cpp
#include "core/sdl_input_manager.h"
```

Add the strong bridge implementation near the other extern-C bridge (search for `coreRuntimeGetActiveNSView` — it should be defined further down in the file, towards the end). Place the new strong def right next to it:

```cpp
extern "C" bool coreRuntimeSetRumbleMotor(void* runtime_opaque,
                                          unsigned port,
                                          unsigned effect,
                                          uint16_t strength) {
    if (!runtime_opaque) return false;
    auto* rt = static_cast<CoreRuntime*>(runtime_opaque);
    auto* sdl = rt->sdlInputManager();
    if (!sdl) return false;
    return sdl->setRumbleMotor(static_cast<int>(port),
                               static_cast<retro_rumble_effect>(effect),
                               strength);
}
```

If the existing `coreRuntimeGetActiveNSView` strong def is missing — search for it; SP3 added it but if it's been refactored, place these together where the NSView one lives now.

### Step 4.8: Zero rumble in `pause()` and `stop()` paths

In `cpp/src/core/libretro/core_runtime.cpp`, find `void CoreRuntime::pause()` (around line 145). After `m_audio.setPaused(true);`, add:

```cpp
    // Stop any active rumble so motors don't keep running while paused.
    if (m_sdlInput) {
        for (int port = 0; port < InputRouter::NUM_PORTS; ++port) {
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_STRONG, 0);
            m_sdlInput->setRumbleMotor(port, RETRO_RUMBLE_WEAK,   0);
        }
    }
```

Find the worker `runLoop()` post-loop teardown — the code that runs after `m_stopRequested` flips true and the per-frame loop exits, before `retro_unload_game`. Add the same zero-rumble block there so a `stop()` (e.g. quit during active rumble) also silences motors. If the teardown structure isn't immediately obvious, search for `retro_unload_game` and add the zero-rumble immediately above its call site.

### Step 4.9: Wire `setSdlInputManager` in `GameSession`

In `cpp/src/core/game_session.cpp`, at the two `setEmulationMode` call sites — line 355 (inside `startEmulator`-style code path) and line 409 (`resumeEmulation`):

Around line 355, the existing code reads:

```cpp
    // Fix 1: Switch SDL input into emulation mode so button events feed the InputRouter
    if (m_sdlInputManager)
        m_sdlInputManager->setEmulationMode(&rt->input());
```

Replace with:

```cpp
    // Fix 1: Switch SDL input into emulation mode so button events feed the InputRouter.
    // SP5.5: also register the manager on the runtime so the rumble bridge can find it.
    if (m_sdlInputManager) {
        rt->setSdlInputManager(m_sdlInputManager);
        m_sdlInputManager->setEmulationMode(&rt->input());
    }
```

Around line 409 in `resumeEmulation`:

```cpp
        if (m_sdlInputManager)
            m_sdlInputManager->setEmulationMode(&m_libretroAdapter->runtime()->input());
```

Replace with:

```cpp
        if (m_sdlInputManager) {
            m_libretroAdapter->runtime()->setSdlInputManager(m_sdlInputManager);
            m_sdlInputManager->setEmulationMode(&m_libretroAdapter->runtime()->input());
        }
```

### Step 4.10: Build sanity check

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 2>&1 | tail -20
```

Expected: build succeeds. Look out for two failure modes:

- `error: invalid use of incomplete type 'class SdlInputManager'` — means the `#include "core/sdl_input_manager.h"` in `core_runtime.cpp` from Step 4.7 wasn't added. The forward declaration in the header is enough for the member pointer, but the cpp file needs the full include to call methods on it.
- Linker error about duplicate `coreRuntimeSetRumbleMotor` — means the strong override needs to be marked `__attribute__((used))` or the weak stub isn't actually `__attribute__((weak))`. Match the existing NSView pattern exactly.

### Step 4.11: Existing-test regression check

Run:

```sh
ctest --test-dir /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64 --output-on-failure 2>&1 | tail -20
```

Expected: all tests pass. `test_environment_callbacks` in particular should still link — that's why the weak stub exists.

### Step 4.12: Commit

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/sdl_input_manager.h \
        cpp/src/core/sdl_input_manager.cpp \
        cpp/src/core/libretro/environment_callbacks.h \
        cpp/src/core/libretro/environment_callbacks.cpp \
        cpp/src/core/libretro/core_runtime.h \
        cpp/src/core/libretro/core_runtime.cpp \
        cpp/src/core/game_session.cpp
git commit -m "feat(input): wire rumble end-to-end

Registers RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE in environment_callbacks
with a static thunk that bridges into SdlInputManager::setRumbleMotor via
a weak/strong bridge function (mirrors the existing NSView pattern). The
bridge resolves the SdlInputManager off CoreRuntime, which GameSession
now wires alongside setEmulationMode at both startEmulator and
resumeEmulation paths.

SdlInputManager caches per-port motor values (PCSX2 fires STRONG and
WEAK as separate calls) and merges them on each SDL_GameControllerRumble
invocation with a fixed kRumbleDurationMs (100ms) — PCSX2 re-sends
updates frequently while rumble is active, so SDL retriggers before
duration expires.

CoreRuntime::pause() and the runLoop post-teardown both zero all motors
so paused/stopped sessions don't leave motors running."
```

---

## Task 5 — Trace gate (`RETRONEST_INPUT_TRACE`)

Adds env-gated diagnostic logs that mirror the SP4.x `RETRONEST_AUDIO_TRACE` pattern. Off by default; enabled with `RETRONEST_INPUT_TRACE=1`. Lightweight, useful for debugging the smoke test in Task 6 if anything misbehaves.

**Files:**

- Modify: `cpp/src/core/sdl_input_manager.cpp` (one helper + one log line in axis-write path)
- Modify: `cpp/src/core/libretro/core_runtime.cpp` (one helper + log lines in trampoline ANALOG branch + strong rumble bridge)

### Step 5.1: Add the tracer helper to `sdl_input_manager.cpp`

In `cpp/src/core/sdl_input_manager.cpp`, near the top of the file (after the existing static helpers, before any namespaces), add:

```cpp
static bool inputTraceEnabled() {
    static const bool v = (std::getenv("RETRONEST_INPUT_TRACE") != nullptr);
    return v;
}
```

Add `#include <cstdlib>` near the top of the file if not already present.

### Step 5.2: Trace `setAxis` writes

In the same file, inside the axis-motion branch from Task 3.3, immediately after the new `m_emulationTarget->setAxis(devIdx, rpAxis, static_cast<int16_t>(value));` line, add:

```cpp
                    if (inputTraceEnabled()) {
                        qDebug("[sdl] port=%d axis=%d raw=%d", devIdx,
                               static_cast<int>(rpAxis), value);
                    }
```

### Step 5.3: Add the tracer helper to `core_runtime.cpp`

In `cpp/src/core/libretro/core_runtime.cpp`, add a new anonymous-namespace helper next to the existing `audioTraceEnabled` (around line 24):

```cpp
bool inputTraceEnabled() {
    static const bool v = (std::getenv("RETRONEST_INPUT_TRACE") != nullptr);
    return v;
}
```

### Step 5.4: Trace ANALOG reads (rate-limited)

In the same file, inside the new `RETRO_DEVICE_ANALOG` branch from Task 2.1, just before `return g_current->m_input.axis(...)`, add a rate-limited log. Replace the trailing two lines of the ANALOG block:

```cpp
        if (a == RetroPadAxis::Count) return 0;
        return g_current->m_input.axis(static_cast<int>(port), a);
    }
```

with:

```cpp
        if (a == RetroPadAxis::Count) return 0;
        const int16_t rd = g_current->m_input.axis(static_cast<int>(port), a);
        if (inputTraceEnabled()) {
            // Rate-limit: log only every Nth read to keep output sane.
            // 6 axes × 60 fps = 360 reads/sec without limit.
            static thread_local int counter = 0;
            if ((counter++ % 60) == 0) {
                qDebug("[input] port=%u axis=%d rd=%d", port,
                       static_cast<int>(a), rd);
            }
        }
        return rd;
    }
```

### Step 5.5: Trace rumble fires

In `cpp/src/core/sdl_input_manager.cpp`, inside `setRumbleMotor` from Step 4.2, immediately before the `SDL_GameControllerRumble(ctrl, low, high, kRumbleDurationMs);` line, add:

```cpp
    if (inputTraceEnabled()) {
        qDebug("[rumble] port=%d low=%u high=%u", port, low, high);
    }
```

### Step 5.6: Build + regression check

Run:

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp && cmake --build build-arm64 2>&1 | tail -10 && \
  ctest --test-dir /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64 --output-on-failure 2>&1 | tail -10
```

Expected: build succeeds, tests pass. With the env var unset, the new code paths are essentially inert (one cached bool read per call).

### Step 5.7: Commit

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/sdl_input_manager.cpp \
        cpp/src/core/libretro/core_runtime.cpp
git commit -m "feat(input): add RETRONEST_INPUT_TRACE diagnostic flag

Env-gated logs at three sites: setAxis writes (SdlInputManager),
RETRO_DEVICE_ANALOG reads (inputStateTrampoline, rate-limited 1/60),
and rumble fires (setRumbleMotor). Off by default; mirrors the SP4.x
RETRONEST_AUDIO_TRACE pattern."
```

---

## Task 6 — R&C 2 smoke test sequence

No code changes. This is the final verification gate before declaring SP5.5 complete.

**Prerequisites:**

- RetroNest built via Task 5.6 with all changes in place.
- A SDL2-compatible gamepad connected (DualShock 4, DualSense, Xbox One, etc).
- Ratchet & Clank 2 ISO/CHD ready and importable into RetroNest.

### Step 6.1: Smoke check — Controller test screen

- [ ] Launch RetroNest. Confirm the gamepad shows up in the controllers list (existing behavior).
- [ ] Start R&C 2 via the PCSX2 libretro core.
- [ ] At the main menu, navigate to Options → Controller test (or equivalent — R&C 2 has a controller-visualization screen in its options).
- [ ] **Move the left stick** — the on-screen left stick visual should track 1:1 with hardware position. Slow movement = slow on-screen movement.
- [ ] **Move the right stick** — same 1:1 tracking.
- [ ] **Pull L2 and R2 partially** — the trigger gauges/bars should track pressure proportionally, not just on/off.
- [ ] **Press every face button** — A/B/X/Y still register (regression check on the SP5 digital path).

If any of the analog tests fail: re-enable `RETRONEST_INPUT_TRACE=1`, relaunch, and check the trace lines. `[sdl]` lines without matching `[input]` lines = write works but read doesn't (Task 2 problem). `[input]` lines without `[sdl]` lines = read works but write doesn't (Task 3 problem). No lines at all on motion = either trace not enabled or `setEmulationMode` not wired.

### Step 6.2: Smoke check — In-game movement and camera

- [ ] Load any save (or start a new game).
- [ ] **Left stick moves Ratchet** in any direction. Push half-way: he should walk; push full: he should run. (R&C 2 has analog walk-vs-run.)
- [ ] **Right stick rotates camera** smoothly. Slow deflection = slow rotation. No jerky/binary movement.

### Step 6.3: Smoke check — Strafe

- [ ] In-game, **hold L2 (or R2)** — Ratchet should strafe. R&C 2's strafe is essentially binary, but the libretro path is what we're verifying: PCSX2 must see non-zero magnitude on the `+L2`/`+R2` analog binding.

### Step 6.4: Smoke check — Rumble

- [ ] Pick up a weapon (e.g. Bomb Glove) and **fire it**. Rumble should fire on every shot. STRONG (low-freq) and WEAK (high-freq) motors should both be active — a DualSense or DualShock 4 makes the difference very obvious.

If no rumble: verify with `RETRONEST_INPUT_TRACE=1` that `[rumble]` lines appear. If they don't, the env handler isn't being called (Task 4.5 problem). If they appear but the controller doesn't rumble, SDL isn't reaching the device (controller hardware issue, or SDL version mismatch — try `SDL_GameControllerHasRumble` in a debug print).

### Step 6.5: Smoke check — Pause silences rumble

- [ ] Begin firing a weapon (continuous fire if available, e.g. Lancer machine-gun-style weapon). With rumble actively firing, **pause** the emulator via the in-game menu (Select+Start or Touchpad).
- [ ] Motors should stop immediately.
- [ ] **Resume** — rumble should resume on the next shot.

### Step 6.6: Smoke check — Regression on existing digital path

- [ ] Quick run-through of buttons that worked in SP5:
  - D-Pad navigation in menus.
  - Cross (Jump), Circle (attack), Square (wrench), Triangle (action).
  - L1/R1 (sidestep / camera reset).
  - Select+Start (in-game menu) — should still open RetroNest's menu without leaking into the game.

If any of these regressed, it's almost certainly the port-arg change in Task 3.2. Confirm `devIdx` is being read correctly — `qDebug() << devIdx;` next to one of the `setButtonPressed` calls will tell you.

### Step 6.7: Declare done

If all of 6.1–6.6 pass, SP5.5 is shipped.

- [ ] Save a project-memory update noting SP5.5 status and any quirks found during smoke testing.
- [ ] Disable `RETRONEST_INPUT_TRACE` for normal runs (no commit needed; it's an env var).

---

## Self-Review Checklist (run after writing this plan)

Already run inline during plan authoring. Notes:

- **Spec coverage:**
  - Analog stick storage + radial deadzone → Task 1 ✓
  - L2/R2 trigger storage + per-axis deadzone → Task 1 ✓
  - `RETRO_DEVICE_ANALOG` trampoline branch → Task 2 ✓
  - Port-2 wiring (use `devIdx` instead of `0`) → Task 3 ✓
  - SDL→`setAxis` magnitude write → Task 3 ✓
  - Rumble env handler + thunk → Task 4 ✓
  - SdlInputManager `setRumbleMotor` + cache → Task 4 ✓
  - CoreRuntime ↔ SdlInputManager wire + GameSession wire-up → Task 4 ✓
  - Pause/stop zero-rumble → Task 4 ✓
  - Trace gate → Task 5 ✓
  - Smoke test sequence → Task 6 ✓

- **Placeholder scan:** No TBDs, TODOs, or hand-wavey "implement error handling" placeholders. All code blocks contain the actual content to paste.

- **Type consistency:** `RetroPadAxis`, `setAxis`/`axis`, `setRumbleMotor`/`retro_rumble_effect`, `kRumbleDurationMs`, `m_rumbleCache`, `m_sdlInput`, `setSdlInputManager`, `sdlInputManager()`, `coreRuntimeSetRumbleMotor` — all referenced consistently across Tasks 1–5.

- **No new test infrastructure:** `test_input_router` target already exists in `cpp/CMakeLists.txt:908-916` and already links `input_router.cpp` — extending the test file adds tests with no CMake changes.

- **Threading model preserved:** Task 1's atomics keep the lock-free Qt-thread-writes / core-thread-reads invariant. Task 4's rumble cache is also atomic. Task 4's pause-zero runs on the worker thread same as the existing audio pause path. No new mutexes anywhere.
