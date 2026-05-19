# Libretro Hotkeys Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a single global hotkey settings page in RetroNest that drives every libretro core uniformly (RetroArch-style action set: save-state slots, fast-forward, pause, menu, reset, mute, ±volume).

**Architecture:** Host-side `HotkeyMatcher` reads keyboard (Qt event filter) and gamepad (existing `InputRouter` snapshot, polled each frame) and dispatches to common action handlers. No core-side changes. Bindings persisted via the existing `AppController::saveHotkey()` API using a sentinel emuId `"_libretro_global"`. Reuses existing `GenericHotkeyPage` widget.

**Tech Stack:** C++17, Qt 6 (Widgets + Core), QtTest, SDL2 (for gamepad via existing `SdlInputManager`), CMake/ctest.

**Spec:** `docs/superpowers/specs/2026-05-19-retronest-libretro-hotkeys-design.md`

---

## File structure

**New files (cpp/src/):**
- `core/libretro/libretro_hotkey_defs.h` — declares `kLibretroHotkeys` (the static `QVector<HotkeyDef>` table) and `LibretroHotkeyId` enum (stable IDs for dispatch).
- `core/libretro/libretro_hotkey_defs.cpp` — definition of `kLibretroHotkeys`.
- `core/libretro/hotkey_matcher.h` / `.cpp` — `HotkeyMatcher` class: binding parse, press-edge detection, combo-modifier suppression, action dispatch.
- `ui/settings/libretro_hotkey_settings_dialog.h` / `.cpp` — thin wrapper that opens `GenericHotkeyPage` with the libretro sentinel emuId.

**New test files (cpp/tests/):**
- `test_libretro_hotkey_defs.cpp` — schema sanity (unique IDs, sensible defaults).
- `test_hotkey_matcher.cpp` — press-edge, conflict, combo suppression, slot cycling.

**Modified files:**
- `cpp/src/ui/app_controller.h` / `.cpp` — add `showLibretroHotkeySettings()`, route hotkey-def lookup for sentinel emuId to `kLibretroHotkeys`.
- `cpp/src/core/game_session.h` / `.cpp` — add `m_currentSaveSlot` with getter/setter and Q_PROPERTY.
- `cpp/src/core/libretro/audio_sink.h` / `.cpp` — add `setMuted(bool)`, `isMuted()`, `setVolume(float 0..1)`, `volume()`, sample-scaling in `writeSamples`.
- `cpp/src/core/libretro/core_runtime.h` / `.cpp` — own `HotkeyMatcher`; hook per-frame tick into the input-trampoline path or `retro_run` driver.
- `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` — skip hotkey category when adapter is libretro.
- `cpp/src/adapters/libretro/mgba_libretro_adapter.cpp` — likewise (if it currently registers a hotkey hub).
- `cpp/CMakeLists.txt` — register new tests; add the new source files to the host lib target.

**NOT modified:**
- Any file under `pcsx2-libretro/` or other core forks.
- `EmulatorAdapter::hotkeyBindingDefs()` interface.
- Standalone adapter hotkey paths (DuckStation, PPSSPP, Dolphin).

**v1 scope cuts (decided during plan write-up — flag for user awareness):**
- **Screenshot deferred.** No host screenshot capture path exists yet. The row is omitted from `kLibretroHotkeys`; add when the underlying capture is built.
- Final action count: **22 rows** (not 23 — Screenshot removed).

---

## Action set (final, v1)

| ID | `HotkeyDef::key` | Default | Group |
|---|---|---|---|
| `ToggleMenu` | `ToggleMenu` | `Keyboard/Escape` | Navigation |
| `FastForwardToggle` | `FastForwardToggle` | `Keyboard/Space` | Speed |
| `FastForwardHold` | `FastForwardHold` | *(unbound)* | Speed |
| `Pause` | `Pause` | `Keyboard/P` | System |
| `Reset` | `Reset` | `Keyboard/H` | System |
| `SaveState` | `SaveState` | `Keyboard/F2` | Save States |
| `LoadState` | `LoadState` | `Keyboard/F4` | Save States |
| `NextSlot` | `NextSlot` | `Keyboard/F6` | Save States |
| `PrevSlot` | `PrevSlot` | `Keyboard/F7` | Save States |
| `SaveStateSlot1`..`5` | (parallel) | `Keyboard/Shift+F2..F6` | Save States |
| `LoadStateSlot1`..`5` | (parallel) | `Keyboard/Shift+F7..F11` | Save States |
| `Mute` | `Mute` | `Keyboard/M` | Audio |
| `VolumeUp` | `VolumeUp` | `Keyboard/+` | Audio |
| `VolumeDown` | `VolumeDown` | `Keyboard/-` | Audio |

Volume step: 10% per press. Mute is a toggle. Max save slot: 5 (matches the table).

---

## Task 1: Define `kLibretroHotkeys` and the action ID enum

**Files:**
- Create: `cpp/src/core/libretro/libretro_hotkey_defs.h`
- Create: `cpp/src/core/libretro/libretro_hotkey_defs.cpp`
- Create: `cpp/tests/test_libretro_hotkey_defs.cpp`
- Modify: `cpp/CMakeLists.txt` — add sources to host lib, add `test_libretro_hotkey_defs` test target.

The sentinel emuId is `"_libretro_global"`. Used everywhere the existing `AppController` hotkey API expects an emuId.

- [ ] **Step 1.1: Write the failing test**

Create `cpp/tests/test_libretro_hotkey_defs.cpp`:

```cpp
#include <QtTest>
#include <QSet>
#include "core/libretro/libretro_hotkey_defs.h"
#include "core/binding_def.h"

class TestLibretroHotkeyDefs : public QObject {
    Q_OBJECT
private slots:
    void allKeysUnique() {
        QSet<QString> seen;
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys) {
            QVERIFY2(!seen.contains(def.key), qPrintable("duplicate key: " + def.key));
            seen.insert(def.key);
        }
    }
    void hasExpectedCount() {
        // 9 base + 5 SaveStateSlot1..5 + 5 LoadStateSlot1..5 + 3 audio = 22
        QCOMPARE(libretro_hotkeys::kLibretroHotkeys.size(), 22);
    }
    void everyDefHasSection() {
        for (const auto& def : libretro_hotkeys::kLibretroHotkeys)
            QCOMPARE(def.section, QStringLiteral("Hotkeys"));
    }
    void sentinelEmuIdIsStable() {
        QCOMPARE(libretro_hotkeys::kSentinelEmuId, QStringLiteral("_libretro_global"));
    }
};

QTEST_APPLESS_MAIN(TestLibretroHotkeyDefs)
#include "test_libretro_hotkey_defs.moc"
```

- [ ] **Step 1.2: Run test, verify it fails**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-x86_64 --target test_libretro_hotkey_defs 2>&1 | tail -20
```

Expected: build error — `libretro_hotkey_defs.h: No such file or directory`.

- [ ] **Step 1.3: Implement the header**

Create `cpp/src/core/libretro/libretro_hotkey_defs.h`:

```cpp
#pragma once
#include <QString>
#include <QVector>
#include "core/binding_def.h"

namespace libretro_hotkeys {

inline const QString kSentinelEmuId = QStringLiteral("_libretro_global");

// Stable IDs used for HotkeyDef::key and for dispatch lookup.
namespace ids {
    inline const QString ToggleMenu          = QStringLiteral("ToggleMenu");
    inline const QString FastForwardToggle   = QStringLiteral("FastForwardToggle");
    inline const QString FastForwardHold     = QStringLiteral("FastForwardHold");
    inline const QString Pause               = QStringLiteral("Pause");
    inline const QString Reset               = QStringLiteral("Reset");
    inline const QString SaveState           = QStringLiteral("SaveState");
    inline const QString LoadState           = QStringLiteral("LoadState");
    inline const QString NextSlot            = QStringLiteral("NextSlot");
    inline const QString PrevSlot            = QStringLiteral("PrevSlot");
    inline const QString Mute                = QStringLiteral("Mute");
    inline const QString VolumeUp            = QStringLiteral("VolumeUp");
    inline const QString VolumeDown          = QStringLiteral("VolumeDown");
    // SaveStateSlotN / LoadStateSlotN constructed on the fly via QString::number.
    inline QString SaveStateSlot(int n) { return QStringLiteral("SaveStateSlot%1").arg(n); }
    inline QString LoadStateSlot(int n) { return QStringLiteral("LoadStateSlot%1").arg(n); }
}

extern const QVector<HotkeyDef> kLibretroHotkeys;

} // namespace libretro_hotkeys
```

- [ ] **Step 1.4: Implement the table**

Create `cpp/src/core/libretro/libretro_hotkey_defs.cpp`:

```cpp
#include "core/libretro/libretro_hotkey_defs.h"

namespace libretro_hotkeys {

static HotkeyDef make(const QString& label, const QString& group,
                      const QString& key, const QString& def) {
    return HotkeyDef{label, group, QStringLiteral("Hotkeys"), key, def};
}

const QVector<HotkeyDef> kLibretroHotkeys = []() {
    QVector<HotkeyDef> v;
    v << make(QStringLiteral("Toggle In-Game Menu"), QStringLiteral("Navigation"),
              ids::ToggleMenu, QStringLiteral("Keyboard/Escape"));
    v << make(QStringLiteral("Fast Forward (Toggle)"), QStringLiteral("Speed"),
              ids::FastForwardToggle, QStringLiteral("Keyboard/Space"));
    v << make(QStringLiteral("Fast Forward (Hold)"), QStringLiteral("Speed"),
              ids::FastForwardHold, QString());
    v << make(QStringLiteral("Pause / Resume"), QStringLiteral("System"),
              ids::Pause, QStringLiteral("Keyboard/P"));
    v << make(QStringLiteral("Reset"), QStringLiteral("System"),
              ids::Reset, QStringLiteral("Keyboard/H"));
    v << make(QStringLiteral("Save State (Current Slot)"), QStringLiteral("Save States"),
              ids::SaveState, QStringLiteral("Keyboard/F2"));
    v << make(QStringLiteral("Load State (Current Slot)"), QStringLiteral("Save States"),
              ids::LoadState, QStringLiteral("Keyboard/F4"));
    v << make(QStringLiteral("Next Save Slot"), QStringLiteral("Save States"),
              ids::NextSlot, QStringLiteral("Keyboard/F6"));
    v << make(QStringLiteral("Previous Save Slot"), QStringLiteral("Save States"),
              ids::PrevSlot, QStringLiteral("Keyboard/F7"));
    for (int n = 1; n <= 5; ++n) {
        v << make(QStringLiteral("Save State to Slot %1").arg(n),
                  QStringLiteral("Save States"),
                  ids::SaveStateSlot(n),
                  QStringLiteral("Keyboard/Shift+F%1").arg(n + 1));
    }
    for (int n = 1; n <= 5; ++n) {
        v << make(QStringLiteral("Load State from Slot %1").arg(n),
                  QStringLiteral("Save States"),
                  ids::LoadStateSlot(n),
                  QStringLiteral("Keyboard/Shift+F%1").arg(n + 6));
    }
    v << make(QStringLiteral("Toggle Mute"), QStringLiteral("Audio"),
              ids::Mute, QStringLiteral("Keyboard/M"));
    v << make(QStringLiteral("Volume Up"), QStringLiteral("Audio"),
              ids::VolumeUp, QStringLiteral("Keyboard/+"));
    v << make(QStringLiteral("Volume Down"), QStringLiteral("Audio"),
              ids::VolumeDown, QStringLiteral("Keyboard/-"));
    return v;
}();

} // namespace libretro_hotkeys
```

- [ ] **Step 1.5: Register in CMake**

Modify `cpp/CMakeLists.txt`:

1. Find the section where host-lib sources are listed (search for `audio_sink.cpp` or `input_router.cpp` — `libretro_hotkey_defs.cpp` joins that list).
2. Add `core/libretro/libretro_hotkey_defs.cpp` to the source list.
3. After the existing `add_test(NAME ... COMMAND test_format_binding)` block (around line 490), add:

```cmake
add_executable(test_libretro_hotkey_defs tests/test_libretro_hotkey_defs.cpp)
target_link_libraries(test_libretro_hotkey_defs PRIVATE retronest_core Qt6::Test)
add_test(NAME LibretroHotkeyDefs COMMAND test_libretro_hotkey_defs)
```

(Adjust target name `retronest_core` to match the actual host-lib target — search `add_library` in this file.)

- [ ] **Step 1.6: Run test, verify it passes**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-x86_64 --target test_libretro_hotkey_defs && \
  ctest --test-dir build-x86_64 -R LibretroHotkeyDefs -V
```

Expected: `PASS` for all four test cases.

- [ ] **Step 1.7: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/core/libretro/libretro_hotkey_defs.h \
        cpp/src/core/libretro/libretro_hotkey_defs.cpp \
        cpp/tests/test_libretro_hotkey_defs.cpp \
        cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(hotkeys): define libretro hotkey schema

22-row RetroArch-style action table consumed by the new global libretro
hotkey page. Uses sentinel emuId "_libretro_global" so existing
AppController hotkey APIs work unchanged.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Wire `AppController` to serve `kLibretroHotkeys` for the sentinel emuId

The existing `AppController::hotkeyBindings(emuId)` currently routes to per-emulator adapter's `hotkeyBindingDefs()`. For the sentinel emuId, return `kLibretroHotkeys` instead. Persistence (`saveHotkey`/`clearHotkey`/`resetHotkeys`) already works generically — it just needs to accept the sentinel emuId.

**Files:**
- Modify: `cpp/src/ui/app_controller.cpp` — `hotkeyBindings()`, `hasHotkeys()`, `saveHotkey()`, etc.
- Create: `cpp/tests/test_app_controller_libretro_hotkeys.cpp`

- [ ] **Step 2.1: Write the failing test**

Create `cpp/tests/test_app_controller_libretro_hotkeys.cpp`:

```cpp
#include <QtTest>
#include <QTemporaryDir>
#include "ui/app_controller.h"
#include "core/libretro/libretro_hotkey_defs.h"

class TestAppControllerLibretroHotkeys : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_tmp;
    AppController* m_ac = nullptr;

private slots:
    void initTestCase() {
        QVERIFY(m_tmp.isValid());
        // Configure AppController to use m_tmp.path() as config root if such an API exists;
        // otherwise rely on QStandardPaths override via env (XDG_CONFIG_HOME=...).
        qputenv("XDG_CONFIG_HOME", m_tmp.path().toUtf8());
        m_ac = new AppController(/* dependencies as test setup requires */);
    }
    void cleanupTestCase() { delete m_ac; }

    void sentinelReturnsLibretroSchema() {
        QVariantList rows = m_ac->hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        QCOMPARE(rows.size(), libretro_hotkeys::kLibretroHotkeys.size());
    }
    void hasHotkeysTrueForSentinel() {
        QVERIFY(m_ac->hasHotkeys(libretro_hotkeys::kSentinelEmuId));
    }
    void saveAndReadBackRoundTrips() {
        m_ac->saveHotkey(libretro_hotkeys::kSentinelEmuId,
                         QStringLiteral("Hotkeys"),
                         libretro_hotkeys::ids::Pause,
                         QStringLiteral("Keyboard/Z"));
        QVariantList rows = m_ac->hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
        bool found = false;
        for (const auto& v : rows) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("key")).toString() == libretro_hotkeys::ids::Pause) {
                QCOMPARE(m.value(QStringLiteral("currentValue")).toString(),
                         QStringLiteral("Keyboard/Z"));
                found = true;
            }
        }
        QVERIFY(found);
    }
};

QTEST_MAIN(TestAppControllerLibretroHotkeys)
#include "test_app_controller_libretro_hotkeys.moc"
```

> NOTE: If `AppController` requires non-trivial construction (input manager, theme, etc.), this test may need a fake/mock — check `cpp/tests/test_app_controller_*.cpp` for the established mock pattern and follow it.

- [ ] **Step 2.2: Run test, verify it fails**

```bash
cmake --build build-x86_64 --target test_app_controller_libretro_hotkeys 2>&1 | tail -10
ctest --test-dir build-x86_64 -R AppControllerLibretroHotkeys -V
```

Expected: test FAILS (sentinel routes to nothing, returns empty list).

- [ ] **Step 2.3: Add the sentinel routing in `AppController`**

In `cpp/src/ui/app_controller.cpp` find `AppController::hotkeyBindings`. The current shape (per the recon) delegates to `m_configService` keyed by emuId. Modify to special-case the sentinel:

```cpp
#include "core/libretro/libretro_hotkey_defs.h"

QVariantList AppController::hotkeyBindings(const QString& emuId) const {
    QVector<HotkeyDef> defs;
    if (emuId == libretro_hotkeys::kSentinelEmuId) {
        defs = libretro_hotkeys::kLibretroHotkeys;
    } else {
        EmulatorAdapter* a = adapterById(emuId);
        if (!a) return {};
        defs = a->hotkeyBindingDefs();
    }
    QVariantList out;
    for (const HotkeyDef& d : defs) {
        QVariantMap m;
        m[QStringLiteral("label")]        = d.label;
        m[QStringLiteral("group")]        = d.group;
        m[QStringLiteral("section")]      = d.section;
        m[QStringLiteral("key")]          = d.key;
        m[QStringLiteral("defaultValue")] = d.defaultValue;
        m[QStringLiteral("currentValue")] = m_configService->readHotkey(emuId, d.section, d.key, d.defaultValue);
        out.push_back(m);
    }
    return out;
}

bool AppController::hasHotkeys(const QString& emuId) const {
    if (emuId == libretro_hotkeys::kSentinelEmuId) return true;
    EmulatorAdapter* a = adapterById(emuId);
    return a && !a->hotkeyBindingDefs().isEmpty();
}
```

> The exact existing shape of `hotkeyBindings()` may differ (it may already build the QVariantMap differently or call into a different config-service method). Adapt the diff to the current code — preserve existing field names. The key change is the *sentinel branch* that returns `kLibretroHotkeys` instead of going to the adapter.

`saveHotkey()`, `clearHotkey()`, and `resetHotkeys()` should require **no changes** because they already accept any emuId string. Verify with a quick grep that they don't validate emuId against the adapter registry.

- [ ] **Step 2.4: Run test, verify it passes**

```bash
cmake --build build-x86_64 --target test_app_controller_libretro_hotkeys && \
  ctest --test-dir build-x86_64 -R AppControllerLibretroHotkeys -V
```

Expected: PASS.

- [ ] **Step 2.5: Add the new test to CMakeLists.txt + commit**

```cmake
add_executable(test_app_controller_libretro_hotkeys tests/test_app_controller_libretro_hotkeys.cpp)
target_link_libraries(test_app_controller_libretro_hotkeys PRIVATE retronest_core Qt6::Test)
add_test(NAME AppControllerLibretroHotkeys COMMAND test_app_controller_libretro_hotkeys)
```

```bash
git add cpp/src/ui/app_controller.cpp cpp/tests/test_app_controller_libretro_hotkeys.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(hotkeys): route AppController::hotkeyBindings sentinel to libretro schema

When emuId == "_libretro_global", AppController returns kLibretroHotkeys
instead of consulting an adapter. Persistence reuses the existing
ConfigService hotkey API unchanged.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: `HotkeyMatcher` — keyboard press-edge detection

Build the matcher incrementally. Start with keyboard-only single-key bindings, no combos. Subsequent tasks layer on combos and gamepad.

**Files:**
- Create: `cpp/src/core/libretro/hotkey_matcher.h`
- Create: `cpp/src/core/libretro/hotkey_matcher.cpp`
- Create: `cpp/tests/test_hotkey_matcher.cpp`

- [ ] **Step 3.1: Write the failing test (keyboard press-edge)**

`cpp/tests/test_hotkey_matcher.cpp`:

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "core/libretro/hotkey_matcher.h"

class TestHotkeyMatcher : public QObject {
    Q_OBJECT
private slots:
    void firesOnKeyPressEdge() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/F1"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);

        m.onKeyEvent(Qt::Key_F1, /*pressed=*/true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("Foo"));

        // Held: no refire while still pressed
        m.onKeyEvent(Qt::Key_F1, /*pressed=*/true);
        QCOMPARE(spy.count(), 0);

        // Release: no actionPressed for non-hold-style binds
        m.onKeyEvent(Qt::Key_F1, /*pressed=*/false);
        QCOMPARE(spy.count(), 0);

        // Press again: fires
        m.onKeyEvent(Qt::Key_F1, /*pressed=*/true);
        QCOMPARE(spy.count(), 1);
    }

    void unboundKeyDoesNothing() {
        HotkeyMatcher m;
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onKeyEvent(Qt::Key_F1, true);
        QCOMPARE(spy.count(), 0);
    }

    void emptyBindingClearsAction() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Keyboard/F1"));
        m.setBinding(QStringLiteral("Foo"), QString());
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onKeyEvent(Qt::Key_F1, true);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_APPLESS_MAIN(TestHotkeyMatcher)
#include "test_hotkey_matcher.moc"
```

- [ ] **Step 3.2: Implement minimal `HotkeyMatcher`**

`cpp/src/core/libretro/hotkey_matcher.h`:

```cpp
#pragma once
#include <QObject>
#include <QHash>
#include <QString>
#include <Qt>

class HotkeyMatcher : public QObject {
    Q_OBJECT
public:
    explicit HotkeyMatcher(QObject* parent = nullptr) : QObject(parent) {}

    // Replace the binding for one action. Empty bindingString clears it.
    // Parses "Keyboard/<KeyName>" (Task 3); "GamepadN/<Button>" and
    // "GamepadN/<Modifier>+<Button>" land in later tasks.
    void setBinding(const QString& actionKey, const QString& bindingString);

    // Drop all bindings.
    void clearAllBindings();

    // Qt keyboard event entry point (called by an event filter).
    void onKeyEvent(int qtKey, bool pressed);

signals:
    // Emitted on the press-edge for non-hold-style actions, and also
    // on press for hold-style actions.
    void actionPressed(const QString& actionKey);

    // Emitted on the release-edge ONLY for hold-style actions.
    // (Determined by the action key being known hold-style — see is_hold().)
    void actionReleased(const QString& actionKey);

private:
    struct KeyBinding { int qtKey; };
    QHash<QString, KeyBinding> m_keyBindings;     // action → key
    QHash<int, QString>        m_keyToAction;     // key → action (reverse lookup)
    QHash<QString, bool>       m_actionPressed;   // current "is held" state per action

    static bool isHoldAction(const QString& actionKey);
};
```

`cpp/src/core/libretro/hotkey_matcher.cpp`:

```cpp
#include "core/libretro/hotkey_matcher.h"
#include <QStringList>
#include <QKeySequence>

static int parseKeyboardSpec(const QString& spec) {
    // spec is e.g. "Keyboard/F1" or "Keyboard/Shift+F2"
    if (!spec.startsWith(QStringLiteral("Keyboard/"))) return 0;
    QString rest = spec.mid(QStringLiteral("Keyboard/").size());
    QKeySequence seq(rest, QKeySequence::PortableText);
    if (seq.count() == 0) return 0;
    return seq[0];  // includes modifier bits (Qt::ShiftModifier | Qt::Key_F2)
}

bool HotkeyMatcher::isHoldAction(const QString& actionKey) {
    return actionKey == QStringLiteral("FastForwardHold");
}

void HotkeyMatcher::setBinding(const QString& actionKey, const QString& bindingString) {
    // Drop any previous binding for this action.
    auto it = m_keyBindings.find(actionKey);
    if (it != m_keyBindings.end()) {
        m_keyToAction.remove(it->qtKey);
        m_keyBindings.erase(it);
    }
    if (bindingString.isEmpty()) return;

    int key = parseKeyboardSpec(bindingString);
    if (key == 0) return;
    m_keyBindings.insert(actionKey, KeyBinding{key});
    m_keyToAction.insert(key, actionKey);
}

void HotkeyMatcher::clearAllBindings() {
    m_keyBindings.clear();
    m_keyToAction.clear();
    m_actionPressed.clear();
}

void HotkeyMatcher::onKeyEvent(int qtKey, bool pressed) {
    auto it = m_keyToAction.find(qtKey);
    if (it == m_keyToAction.end()) return;
    const QString& action = it.value();
    const bool wasPressed = m_actionPressed.value(action, false);
    if (pressed && !wasPressed) {
        m_actionPressed[action] = true;
        emit actionPressed(action);
    } else if (!pressed && wasPressed) {
        m_actionPressed[action] = false;
        if (isHoldAction(action)) emit actionReleased(action);
    }
}
```

- [ ] **Step 3.3: Register source + test in CMakeLists.txt; build + run**

Add `core/libretro/hotkey_matcher.cpp` to the host-lib sources.

```cmake
add_executable(test_hotkey_matcher tests/test_hotkey_matcher.cpp)
target_link_libraries(test_hotkey_matcher PRIVATE retronest_core Qt6::Test)
add_test(NAME HotkeyMatcher COMMAND test_hotkey_matcher)
```

```bash
cmake --build build-x86_64 --target test_hotkey_matcher && \
  ctest --test-dir build-x86_64 -R HotkeyMatcher -V
```

Expected: all 3 tests PASS.

- [ ] **Step 3.4: Commit**

```bash
git add cpp/src/core/libretro/hotkey_matcher.h cpp/src/core/libretro/hotkey_matcher.cpp \
        cpp/tests/test_hotkey_matcher.cpp cpp/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(hotkeys): HotkeyMatcher keyboard press-edge dispatch

Initial slice: keyboard-only bindings with press-edge detection, no
combos. Emits actionPressed on transition false→true, actionReleased
on true→false for hold-style actions (FastForwardHold).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: `HotkeyMatcher` — gamepad single-button bindings

**Files:**
- Modify: `cpp/src/core/libretro/hotkey_matcher.h` / `.cpp`
- Modify: `cpp/tests/test_hotkey_matcher.cpp`

`InputRouter` already tracks per-port `RetroPadButton` state. The matcher receives gamepad state via a `void onGamepadButton(int port, int button, bool pressed)` entry point, called from wherever the host already detects button transitions (likely a poll callback in `SdlInputManager` or `InputRouter`).

- [ ] **Step 4.1: Extend test**

Append to `cpp/tests/test_hotkey_matcher.cpp` (inside the test class):

```cpp
    void firesOnGamepadButtonPressEdge() {
        HotkeyMatcher m;
        // Binding string format: "Gamepad<port>/<RetroPadButton enum int>"
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/8"));  // 8 = R3 say
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 1);
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 1);
        m.onGamepadButton(0, 8, false);
        m.onGamepadButton(0, 8, true);
        QCOMPARE(spy.count(), 2);
    }
    void gamepadPortIsolated() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad1/8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);
        m.onGamepadButton(0, 8, true);  // wrong port
        QCOMPARE(spy.count(), 0);
        m.onGamepadButton(1, 8, true);  // right port
        QCOMPARE(spy.count(), 1);
    }
```

- [ ] **Step 4.2: Run, verify fails**

```bash
ctest --test-dir build-x86_64 -R HotkeyMatcher -V
```

Expected: 2 new tests FAIL (function `onGamepadButton` doesn't exist).

- [ ] **Step 4.3: Extend the matcher header + impl**

Add to `hotkey_matcher.h`:

```cpp
    void onGamepadButton(int port, int button, bool pressed);

private:
    struct GamepadBinding { int port; int button; };
    QHash<QString, GamepadBinding> m_padBindings;
    QHash<qint64, QString>         m_padToAction;  // key = (port<<32)|button

    static qint64 padKey(int port, int button) {
        return (qint64(port) << 32) | qint64(button);
    }
```

Add to `hotkey_matcher.cpp`:

```cpp
static bool parseGamepadSpec(const QString& spec, int* port, int* btn) {
    // "Gamepad<P>/<B>"
    if (!spec.startsWith(QStringLiteral("Gamepad"))) return false;
    int slash = spec.indexOf(QLatin1Char('/'));
    if (slash < 0) return false;
    bool ok1 = false, ok2 = false;
    *port = spec.mid(7, slash - 7).toInt(&ok1);
    *btn = spec.mid(slash + 1).toInt(&ok2);
    return ok1 && ok2;
}
```

Update `setBinding()` to try gamepad parse before falling back to keyboard:

```cpp
void HotkeyMatcher::setBinding(const QString& actionKey, const QString& bindingString) {
    // Drop previous bindings of either type for this action.
    if (auto it = m_keyBindings.find(actionKey); it != m_keyBindings.end()) {
        m_keyToAction.remove(it->qtKey);
        m_keyBindings.erase(it);
    }
    if (auto it = m_padBindings.find(actionKey); it != m_padBindings.end()) {
        m_padToAction.remove(padKey(it->port, it->button));
        m_padBindings.erase(it);
    }
    if (bindingString.isEmpty()) return;

    int port, btn;
    if (parseGamepadSpec(bindingString, &port, &btn)) {
        m_padBindings.insert(actionKey, GamepadBinding{port, btn});
        m_padToAction.insert(padKey(port, btn), actionKey);
        return;
    }
    int key = parseKeyboardSpec(bindingString);
    if (key == 0) return;
    m_keyBindings.insert(actionKey, KeyBinding{key});
    m_keyToAction.insert(key, actionKey);
}

void HotkeyMatcher::onGamepadButton(int port, int button, bool pressed) {
    auto it = m_padToAction.find(padKey(port, button));
    if (it == m_padToAction.end()) return;
    const QString& action = it.value();
    const bool wasPressed = m_actionPressed.value(action, false);
    if (pressed && !wasPressed) {
        m_actionPressed[action] = true;
        emit actionPressed(action);
    } else if (!pressed && wasPressed) {
        m_actionPressed[action] = false;
        if (isHoldAction(action)) emit actionReleased(action);
    }
}
```

Also update `clearAllBindings()` to clear pad maps.

- [ ] **Step 4.4: Run, verify passes; commit**

```bash
ctest --test-dir build-x86_64 -R HotkeyMatcher -V
```

```bash
git add cpp/src/core/libretro/hotkey_matcher.h cpp/src/core/libretro/hotkey_matcher.cpp \
        cpp/tests/test_hotkey_matcher.cpp
git commit -m "feat(hotkeys): HotkeyMatcher supports single-button gamepad bindings\n\nCo-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: `HotkeyMatcher` — gamepad combos with modifier suppression

A binding like `Gamepad0/4+8` (button 4 = modifier, 8 = action) fires the action when both held *and the action is the most recent press*, and suppresses the modifier button from passing through to the game while held.

The host queries "is this modifier currently being used as a combo modifier?" via `bool isSuppressed(int port, int button)`. The input router checks this before delivering gamepad state to the core trampoline.

- [ ] **Step 5.1: Extend test**

Append:

```cpp
    void comboFiresAndSuppresses() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        QSignalSpy spy(&m, &HotkeyMatcher::actionPressed);

        m.onGamepadButton(0, 4, true);   // modifier down
        QVERIFY(!m.isSuppressed(0, 4));  // no combo matched yet
        QCOMPARE(spy.count(), 0);

        m.onGamepadButton(0, 8, true);   // action down WHILE modifier held
        QCOMPARE(spy.count(), 1);
        QVERIFY(m.isSuppressed(0, 4));   // modifier now suppressed

        m.onGamepadButton(0, 8, false);
        m.onGamepadButton(0, 4, false);
        QVERIFY(!m.isSuppressed(0, 4));  // cleared on modifier release
    }
    void modifierAloneDoesNotSuppress() {
        HotkeyMatcher m;
        m.setBinding(QStringLiteral("Foo"), QStringLiteral("Gamepad0/4+8"));
        m.onGamepadButton(0, 4, true);
        QVERIFY(!m.isSuppressed(0, 4));
    }
```

- [ ] **Step 5.2: Run, verify fails; implement; verify passes; commit**

Implementation outline in `hotkey_matcher.cpp`:

```cpp
// Extend GamepadBinding to optionally carry a modifier:
struct GamepadBinding { int port; int modifier; int button; };  // modifier == -1 if no combo

// parseGamepadSpec handles "Gamepad<P>/<M>+<B>" form:
static bool parseGamepadSpec(const QString& spec, int* port, int* mod, int* btn) {
    if (!spec.startsWith(QStringLiteral("Gamepad"))) return false;
    int slash = spec.indexOf(QLatin1Char('/'));
    if (slash < 0) return false;
    bool okP = false; *port = spec.mid(7, slash - 7).toInt(&okP);
    if (!okP) return false;
    QString rest = spec.mid(slash + 1);
    int plus = rest.indexOf(QLatin1Char('+'));
    if (plus < 0) {
        *mod = -1;
        bool okB = false; *btn = rest.toInt(&okB);
        return okB;
    }
    bool okM = false, okB = false;
    *mod = rest.left(plus).toInt(&okM);
    *btn = rest.mid(plus + 1).toInt(&okB);
    return okM && okB;
}
```

Track per-port held buttons in a `QHash<int, QSet<int>> m_padHeld`. On `onGamepadButton`:
- Update `m_padHeld`.
- For each binding that uses this button as the *action* button, check if the modifier is currently held in `m_padHeld[port]`; if yes and not previously matched, fire `actionPressed` and mark the modifier as `m_suppressed.insert(padKey(port, modifier))`.
- For each binding that uses this button as the *modifier*, on release clear suppression for that port+modifier.

`isSuppressed(port, button)` returns `m_suppressed.contains(padKey(port, button))`.

Edge case: single-button gamepad bindings (modifier == -1) **must not** populate suppression — only combos do.

Commit after tests pass:

```bash
git commit -m "feat(hotkeys): HotkeyMatcher gamepad combos + modifier suppression\n\nCo-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: AudioSink — `setMuted`/`setVolume` plumbing

**Files:**
- Modify: `cpp/src/core/libretro/audio_sink.h`
- Modify: `cpp/src/core/libretro/audio_sink.cpp`
- Create: `cpp/tests/test_audio_sink_mute_volume.cpp`

- [ ] **Step 6.1: Write the failing test**

```cpp
#include <QtTest>
#include "core/libretro/audio_sink.h"

class TestAudioSinkMuteVolume : public QObject {
    Q_OBJECT
private slots:
    void mutedSamplesAreZero() {
        AudioSink s;
        s.setMuted(true);
        int16_t in[8]  = {1000, -1000, 500, -500, 800, -800, 200, -200};
        int16_t out[8] = {0};
        int written = s.applyGainAndMute(in, out, 4);  // 4 stereo frames = 8 samples
        QCOMPARE(written, 4);
        for (int i = 0; i < 8; ++i) QCOMPARE(out[i], int16_t(0));
    }
    void volumeScalesSamples() {
        AudioSink s;
        s.setMuted(false);
        s.setVolume(0.5f);
        int16_t in[2]  = {1000, -1000};
        int16_t out[2] = {0};
        s.applyGainAndMute(in, out, 1);  // 1 stereo frame
        QCOMPARE(out[0], int16_t(500));
        QCOMPARE(out[1], int16_t(-500));
    }
    void volumeClampsTo01() {
        AudioSink s;
        s.setVolume(2.0f); QCOMPARE(s.volume(), 1.0f);
        s.setVolume(-1.0f); QCOMPARE(s.volume(), 0.0f);
    }
};
QTEST_APPLESS_MAIN(TestAudioSinkMuteVolume)
#include "test_audio_sink_mute_volume.moc"
```

- [ ] **Step 6.2: Run, verify fails**

Expected: compile error — `setMuted`/`setVolume`/`applyGainAndMute` don't exist.

- [ ] **Step 6.3: Implement on `AudioSink`**

In `audio_sink.h` (in the public section):

```cpp
void setMuted(bool m);
bool isMuted() const { return m_muted.load(); }
void setVolume(float v);  // clamped to [0.0, 1.0]
float volume() const { return m_volume.load(); }

// Helper used by writeSamples and exposed for unit tests.
// Reads m_muted + m_volume, writes scaled samples to `out`. Returns frames written.
int applyGainAndMute(const int16_t* in, int16_t* out, int frames);

private:
    std::atomic<bool>  m_muted{false};
    std::atomic<float> m_volume{1.0f};
```

In `audio_sink.cpp`:

```cpp
void AudioSink::setMuted(bool m) { m_muted.store(m); }
void AudioSink::setVolume(float v) {
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    m_volume.store(v);
}
int AudioSink::applyGainAndMute(const int16_t* in, int16_t* out, int frames) {
    const int samples = frames * 2;  // assume stereo
    if (m_muted.load()) {
        std::memset(out, 0, sizeof(int16_t) * samples);
        return frames;
    }
    const float g = m_volume.load();
    for (int i = 0; i < samples; ++i) {
        out[i] = int16_t(std::clamp(int(in[i] * g), -32768, 32767));
    }
    return frames;
}
```

Then route `writeSamples()` through `applyGainAndMute()` before pushing to SDL:

```cpp
int AudioSink::writeSamples(const int16_t* data, int frames) {
    // [existing 150ms-cap / backpressure logic stays unchanged]
    if (m_muted.load() || m_volume.load() < 1.0f) {
        thread_local std::vector<int16_t> scratch;
        scratch.resize(frames * 2);
        applyGainAndMute(data, scratch.data(), frames);
        // [existing SDL_QueueAudio call but with scratch.data()]
    } else {
        // [existing SDL_QueueAudio call with data directly]
    }
    // [existing return]
}
```

- [ ] **Step 6.4: Run, verify passes; commit**

```bash
ctest --test-dir build-x86_64 -R AudioSinkMuteVolume -V
```

Add the test target to CMakeLists.txt and commit:

```bash
git commit -m "$(cat <<'EOF'
feat(audio): AudioSink mute + volume

Adds setMuted/setVolume/applyGainAndMute. writeSamples routes through
gain-and-mute when either is non-default; bypasses the scratch buffer
on the all-default fast path. Atomic state for lock-free read on the
audio worker thread.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: `GameSession` — current save slot getter/setter

**Files:**
- Modify: `cpp/src/core/game_session.h`
- Modify: `cpp/src/core/game_session.cpp`
- Create: `cpp/tests/test_game_session_save_slot.cpp` (mock-based, similar pattern to existing GameSession tests)

- [ ] **Step 7.1: Write the failing test**

```cpp
#include <QtTest>
#include "core/game_session.h"

class TestGameSessionSaveSlot : public QObject {
    Q_OBJECT
private slots:
    void defaultSlotIsOne() {
        GameSession s;
        QCOMPARE(s.currentSaveSlot(), 1);
    }
    void setterClampsToOneFive() {
        GameSession s;
        s.setCurrentSaveSlot(3);  QCOMPARE(s.currentSaveSlot(), 3);
        s.setCurrentSaveSlot(0);  QCOMPARE(s.currentSaveSlot(), 1);
        s.setCurrentSaveSlot(99); QCOMPARE(s.currentSaveSlot(), 5);
    }
    void emitsChangeSignal() {
        GameSession s;
        QSignalSpy spy(&s, &GameSession::currentSaveSlotChanged);
        s.setCurrentSaveSlot(3);
        QCOMPARE(spy.count(), 1);
        s.setCurrentSaveSlot(3);  // same value, no emit
        QCOMPARE(spy.count(), 1);
    }
};
QTEST_MAIN(TestGameSessionSaveSlot)
#include "test_game_session_save_slot.moc"
```

- [ ] **Step 7.2: Implement on `GameSession`**

In `game_session.h` (around the existing save-state declarations):

```cpp
    Q_PROPERTY(int currentSaveSlot READ currentSaveSlot WRITE setCurrentSaveSlot NOTIFY currentSaveSlotChanged)

    int currentSaveSlot() const { return m_currentSaveSlot; }
    Q_INVOKABLE void setCurrentSaveSlot(int slot);

signals:
    void currentSaveSlotChanged();

private:
    int m_currentSaveSlot = 1;
```

In `game_session.cpp`:

```cpp
void GameSession::setCurrentSaveSlot(int slot) {
    slot = std::clamp(slot, 1, 5);
    if (slot == m_currentSaveSlot) return;
    m_currentSaveSlot = slot;
    emit currentSaveSlotChanged();
}
```

- [ ] **Step 7.3: Verify pass + commit**

```bash
git commit -m "feat(session): GameSession.currentSaveSlot 1..5 with Q_PROPERTY\n\nCo-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 8: Action dispatcher — wire matcher signals to handlers

**Files:**
- Create: `cpp/src/core/libretro/hotkey_dispatcher.h`
- Create: `cpp/src/core/libretro/hotkey_dispatcher.cpp`
- Create: `cpp/tests/test_hotkey_dispatcher.cpp`

The dispatcher is a thin object that listens to `HotkeyMatcher::actionPressed` and `actionReleased` and routes them to the right service:
- `GameSession*` for save/load state, FF, pause/resume, slot cycling.
- `CoreRuntime*` for reset.
- `AudioSink*` for mute / ±volume.
- `AppController*` for `openInGameMenuPanel()`.

- [ ] **Step 8.1: Write the failing test (using mocks)**

```cpp
#include <QtTest>
#include "core/libretro/hotkey_dispatcher.h"
#include "core/libretro/libretro_hotkey_defs.h"

// Minimal mocks — record calls.
struct MockGameSession {
    int saves = 0, loads = 0, lastSlot = -1;
    int ffToggles = 0;
    bool currentSaveSlotIncremented = false;
    void saveStateLibretro(int s) { saves++; lastSlot = s; }
    void loadStateLibretro(int s) { loads++; lastSlot = s; }
    bool toggleFastForwardLibretro() { ffToggles++; return true; }
    int  currentSaveSlot() const { return 2; }  // pretend "current slot" = 2
    void setCurrentSaveSlot(int) { currentSaveSlotIncremented = true; }
};

class TestHotkeyDispatcher : public QObject {
    Q_OBJECT
private slots:
    void saveStateUsesCurrentSlot() {
        MockGameSession gs;
        HotkeyDispatcher d(&gs, /*runtime=*/nullptr, /*audio=*/nullptr, /*app=*/nullptr);
        d.onActionPressed(libretro_hotkeys::ids::SaveState);
        QCOMPARE(gs.saves, 1);
        QCOMPARE(gs.lastSlot, 2);  // matches MockGameSession::currentSaveSlot()
    }
    void saveStateSlot3UsesExplicitSlot() {
        MockGameSession gs;
        HotkeyDispatcher d(&gs, nullptr, nullptr, nullptr);
        d.onActionPressed(libretro_hotkeys::ids::SaveStateSlot(3));
        QCOMPARE(gs.saves, 1);
        QCOMPARE(gs.lastSlot, 3);
    }
    void unknownActionIgnored() {
        MockGameSession gs;
        HotkeyDispatcher d(&gs, nullptr, nullptr, nullptr);
        d.onActionPressed(QStringLiteral("Nonsense"));
        QCOMPARE(gs.saves, 0);
    }
    void fastForwardToggleCallsToggle() {
        MockGameSession gs;
        HotkeyDispatcher d(&gs, nullptr, nullptr, nullptr);
        d.onActionPressed(libretro_hotkeys::ids::FastForwardToggle);
        QCOMPARE(gs.ffToggles, 1);
    }
};
QTEST_APPLESS_MAIN(TestHotkeyDispatcher)
#include "test_hotkey_dispatcher.moc"
```

> The dispatcher should accept *concrete pointers*, not virtual interfaces, to avoid adding interfaces to GameSession/CoreRuntime/AudioSink. Use template indirection or a `void*` cast OR — cleaner — pass `std::function` callbacks at construction time so the dispatcher is fully unit-testable. Pick the option closest to existing patterns: search `cpp/src/core/libretro/` for `std::function` use; if absent, use direct pointers and just construct mocks in tests by inheriting from a thin base.

- [ ] **Step 8.2: Implement `HotkeyDispatcher`**

`hotkey_dispatcher.h` (using std::function for testability):

```cpp
#pragma once
#include <QObject>
#include <QString>
#include <functional>

class HotkeyDispatcher : public QObject {
    Q_OBJECT
public:
    struct Callbacks {
        std::function<void(int)>     saveStateSlot;        // (slot)
        std::function<void(int)>     loadStateSlot;        // (slot)
        std::function<int()>         getCurrentSlot;
        std::function<void(int)>     setCurrentSlot;
        std::function<void()>        toggleFastForward;
        std::function<void(bool)>    setFastForward;       // for hold-style
        std::function<void()>        togglePause;
        std::function<void()>        reset;
        std::function<void()>        openMenu;
        std::function<void()>        toggleMute;
        std::function<void(int)>     adjustVolume;         // (+10 or -10 per call)
    };

    explicit HotkeyDispatcher(Callbacks cb, QObject* parent = nullptr);

public slots:
    void onActionPressed(const QString& actionKey);
    void onActionReleased(const QString& actionKey);

private:
    Callbacks m_cb;
};
```

`hotkey_dispatcher.cpp` — handle each action ID:

```cpp
#include "core/libretro/hotkey_dispatcher.h"
#include "core/libretro/libretro_hotkey_defs.h"
namespace ids = libretro_hotkeys::ids;

HotkeyDispatcher::HotkeyDispatcher(Callbacks cb, QObject* p)
    : QObject(p), m_cb(std::move(cb)) {}

void HotkeyDispatcher::onActionPressed(const QString& a) {
    if (a == ids::ToggleMenu) { if (m_cb.openMenu) m_cb.openMenu(); return; }
    if (a == ids::FastForwardToggle) { if (m_cb.toggleFastForward) m_cb.toggleFastForward(); return; }
    if (a == ids::FastForwardHold)   { if (m_cb.setFastForward) m_cb.setFastForward(true); return; }
    if (a == ids::Pause) { if (m_cb.togglePause) m_cb.togglePause(); return; }
    if (a == ids::Reset) { if (m_cb.reset) m_cb.reset(); return; }
    if (a == ids::SaveState) {
        if (m_cb.saveStateSlot && m_cb.getCurrentSlot) m_cb.saveStateSlot(m_cb.getCurrentSlot());
        return;
    }
    if (a == ids::LoadState) {
        if (m_cb.loadStateSlot && m_cb.getCurrentSlot) m_cb.loadStateSlot(m_cb.getCurrentSlot());
        return;
    }
    if (a == ids::NextSlot) {
        if (m_cb.getCurrentSlot && m_cb.setCurrentSlot) m_cb.setCurrentSlot(m_cb.getCurrentSlot() + 1);
        return;
    }
    if (a == ids::PrevSlot) {
        if (m_cb.getCurrentSlot && m_cb.setCurrentSlot) m_cb.setCurrentSlot(m_cb.getCurrentSlot() - 1);
        return;
    }
    for (int n = 1; n <= 5; ++n) {
        if (a == ids::SaveStateSlot(n)) { if (m_cb.saveStateSlot) m_cb.saveStateSlot(n); return; }
        if (a == ids::LoadStateSlot(n)) { if (m_cb.loadStateSlot) m_cb.loadStateSlot(n); return; }
    }
    if (a == ids::Mute) { if (m_cb.toggleMute) m_cb.toggleMute(); return; }
    if (a == ids::VolumeUp)   { if (m_cb.adjustVolume) m_cb.adjustVolume(+10); return; }
    if (a == ids::VolumeDown) { if (m_cb.adjustVolume) m_cb.adjustVolume(-10); return; }
}

void HotkeyDispatcher::onActionReleased(const QString& a) {
    if (a == ids::FastForwardHold && m_cb.setFastForward) m_cb.setFastForward(false);
}
```

Update the test to use the `Callbacks` pattern instead of MockGameSession.

- [ ] **Step 8.3: Verify pass + commit**

```bash
git commit -m "$(cat <<'EOF'
feat(hotkeys): HotkeyDispatcher routes matcher signals to actions

Action ID → side-effect routing via injected std::function callbacks
to keep the dispatcher unit-testable without coupling to concrete
GameSession/AudioSink/CoreRuntime types.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Wire matcher + dispatcher into `CoreRuntime` + `AppController`

**Files:**
- Modify: `cpp/src/core/libretro/core_runtime.h` — own `HotkeyMatcher` and `HotkeyDispatcher`.
- Modify: `cpp/src/core/libretro/core_runtime.cpp` — instantiate at startup, hook into input flow, connect matcher signals to dispatcher slots, wire dispatcher callbacks to GameSession/AudioSink/AppController.
- Modify: `cpp/src/ui/app_controller.cpp` — install QApplication keyboard event filter when a libretro game starts; load bindings via `hotkeyBindings(kSentinelEmuId)` and push to matcher.

This task has no unit tests by itself — it's pure integration. Verification is the manual smoke at Task 12.

- [ ] **Step 9.1: Instantiate matcher + dispatcher**

In `core_runtime.h` add members:

```cpp
private:
    std::unique_ptr<HotkeyMatcher>    m_hotkeyMatcher;
    std::unique_ptr<HotkeyDispatcher> m_hotkeyDispatcher;
```

In `core_runtime.cpp` constructor: create both, wire signals:

```cpp
m_hotkeyMatcher = std::make_unique<HotkeyMatcher>();

HotkeyDispatcher::Callbacks cb;
cb.saveStateSlot     = [this](int s) { m_gameSession->saveStateLibretro(s); };
cb.loadStateSlot     = [this](int s) { m_gameSession->loadStateLibretro(s); };
cb.getCurrentSlot    = [this]()      { return m_gameSession->currentSaveSlot(); };
cb.setCurrentSlot    = [this](int s) {
    m_gameSession->setCurrentSaveSlot(s);
    emit raInfoToast(QStringLiteral("Save State"),
                     QStringLiteral("Slot %1").arg(m_gameSession->currentSaveSlot()),
                     QString(), QString(), 1500);
};
cb.toggleFastForward = [this]()      { m_gameSession->toggleFastForwardLibretro(); };
cb.setFastForward    = [this](bool on) { /* host speed mult; see toggle impl */ };
cb.togglePause       = [this]()      { isPaused() ? resume() : pause(); };
cb.reset             = [this]()      { reset(); };
cb.openMenu          = [this]()      { emit requestOpenInGameMenu(); };  // new signal connected to AppController
cb.toggleMute        = [this]()      { m_audioSink->setMuted(!m_audioSink->isMuted()); };
cb.adjustVolume      = [this](int dPct) {
    float v = m_audioSink->volume() + (dPct / 100.0f);
    m_audioSink->setVolume(v);
};

m_hotkeyDispatcher = std::make_unique<HotkeyDispatcher>(cb);
connect(m_hotkeyMatcher.get(), &HotkeyMatcher::actionPressed,
        m_hotkeyDispatcher.get(), &HotkeyDispatcher::onActionPressed);
connect(m_hotkeyMatcher.get(), &HotkeyMatcher::actionReleased,
        m_hotkeyDispatcher.get(), &HotkeyDispatcher::onActionReleased);
```

Add `signals: void requestOpenInGameMenu();` to `CoreRuntime`. In `AppController`, connect this to `openInGameMenuPanel()`.

- [ ] **Step 9.2: Hook gamepad poll into matcher**

`inputStateTrampoline` is the per-query call site. We don't want to walk every binding on every query (24 device queries × 60 fps × per-binding loop = too much). Instead, hook into the place where `RetroPadState` is *updated* (the SDL→atomic write path in `InputRouter` or `SdlInputManager`). The matcher receives only state *transitions*, not query polls.

Locate the SDL callback that writes to `InputRouter::m_buttons[port]`. After the write, compute the delta vs the previous frame's bitmask and call `m_hotkeyMatcher->onGamepadButton(port, btn, pressed)` for each changed bit. (One per port per frame is cheap.)

If no convenient delta-detection layer exists, add it inside `InputRouter` as a private helper and emit a `Q_SIGNAL void buttonStateChanged(int port, int button, bool pressed)` — then `CoreRuntime` connects matcher's `onGamepadButton` to it.

- [ ] **Step 9.3: Hook keyboard events into matcher**

In `AppController` (or wherever the main `QApplication` lives), install an event filter active while a libretro game runs:

```cpp
class LibretroKeyFilter : public QObject {
public:
    LibretroKeyFilter(HotkeyMatcher* m) : m_m(m) {}
    bool eventFilter(QObject* o, QEvent* e) override {
        if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
            auto* k = static_cast<QKeyEvent*>(e);
            if (!k->isAutoRepeat())
                m_m->onKeyEvent(k->key() | int(k->modifiers()),
                                e->type() == QEvent::KeyPress);
        }
        return false;
    }
private:
    HotkeyMatcher* m_m;
};
```

`AppController` installs the filter on `qApp` when game starts; removes when game ends.

- [ ] **Step 9.4: Push current bindings to matcher on game start + on settings save**

`AppController` adds a helper:

```cpp
void AppController::syncLibretroHotkeyBindings() {
    if (!m_runtime || !m_runtime->hotkeyMatcher()) return;
    m_runtime->hotkeyMatcher()->clearAllBindings();
    QVariantList rows = hotkeyBindings(libretro_hotkeys::kSentinelEmuId);
    for (const QVariant& v : rows) {
        QVariantMap m = v.toMap();
        m_runtime->hotkeyMatcher()->setBinding(
            m.value(QStringLiteral("key")).toString(),
            m.value(QStringLiteral("currentValue")).toString());
    }
}
```

Call `syncLibretroHotkeyBindings()`:
- On libretro game start (right after `GameSession::startLibretro` succeeds).
- After `saveHotkey()` / `clearHotkey()` / `resetHotkeys()` returns (if emuId == sentinel).

Expose `CoreRuntime::hotkeyMatcher()` getter.

- [ ] **Step 9.5: Build the host, manual sanity check**

```bash
cmake --build build-x86_64 -j 2>&1 | grep -E "error|Built target" | tail -10
```

Expected: clean build.

Run RetroNest, start a libretro game (mGBA or PCSX2 once Task 11 lands), press F2 → no crash. Full smoke at Task 12.

- [ ] **Step 9.6: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(hotkeys): integrate matcher + dispatcher into CoreRuntime

CoreRuntime owns HotkeyMatcher and HotkeyDispatcher; keyboard events
flow via QApplication event filter; gamepad transitions flow via
SdlInputManager/InputRouter signal. AppController syncs bindings on
game start and on UI save.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Apply combo-modifier suppression to outgoing gamepad state

The matcher tracks suppressed buttons (Task 5). The core trampoline must NOT report a suppressed button as pressed to the libretro core.

**Files:**
- Modify: `cpp/src/core/libretro/core_runtime.cpp` — inside `inputStateTrampoline`, mask suppressed buttons.

- [ ] **Step 10.1: Modify the trampoline**

Locate `inputStateTrampoline(unsigned port, unsigned device, unsigned index, unsigned id)`. Around the existing `InputRouter::buttonPressed()` call (per recon at line 114–144 of `core_runtime.cpp`):

```cpp
if (device == RETRO_DEVICE_JOYPAD && index == 0) {
    if (g_current->m_hotkeyMatcher &&
        g_current->m_hotkeyMatcher->isSuppressed(int(port), int(id))) {
        return 0;
    }
    return g_current->m_inputRouter->buttonPressed(port, id) ? 1 : 0;
}
```

- [ ] **Step 10.2: Build + check; commit**

```bash
git commit -m "feat(hotkeys): suppress combo-modifier buttons from libretro core input\n\nCo-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 11: Global libretro hotkey settings dialog + AppController route

**Files:**
- Create: `cpp/src/ui/settings/libretro_hotkey_settings_dialog.h` / `.cpp`
- Modify: `cpp/src/ui/app_controller.h` / `.cpp` — add `Q_INVOKABLE void showLibretroHotkeySettings()`.

- [ ] **Step 11.1: Create the dialog**

`libretro_hotkey_settings_dialog.h`:

```cpp
#pragma once
#include "ui/settings/emulator_settings_dialog_base.h"

class SdlInputManager;
class AppController;

class LibretroHotkeySettingsDialog : public EmulatorSettingsDialogBase {
    Q_OBJECT
public:
    LibretroHotkeySettingsDialog(SdlInputManager* inputManager,
                                 AppController* appController,
                                 QWidget* parent = nullptr);
};
```

`.cpp` — copy the relevant portion of `HotkeySettingsDialog` and pass the sentinel emuId to `GenericHotkeyPage`:

```cpp
#include "ui/settings/libretro_hotkey_settings_dialog.h"
#include "ui/settings/generic_hotkey_page.h"
#include "core/libretro/libretro_hotkey_defs.h"

LibretroHotkeySettingsDialog::LibretroHotkeySettingsDialog(
    SdlInputManager* im, AppController* ac, QWidget* parent)
    : EmulatorSettingsDialogBase(parent)
{
    setWindowTitle(tr("Libretro Hotkeys"));
    auto* page = new GenericHotkeyPage(im, ac, libretro_hotkeys::kSentinelEmuId, this);
    setHub(page);  // GenericHotkeyPage acts as its own hub for this dialog
    // [Copy face-button shortcut wiring from HotkeySettingsDialog as-is]
}
```

- [ ] **Step 11.2: Add `AppController::showLibretroHotkeySettings()`**

In `app_controller.h`:

```cpp
Q_INVOKABLE void showLibretroHotkeySettings();
```

In `app_controller.cpp`:

```cpp
void AppController::showLibretroHotkeySettings() {
    if (!m_inputManager) { qWarning() << "no SdlInputManager"; return; }
    auto* dialog = new LibretroHotkeySettingsDialog(m_inputManager, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}
```

- [ ] **Step 11.3: Wire QML entry point**

In whichever main-settings QML view RetroNest uses, add a "Libretro Hotkeys" row that calls `appController.showLibretroHotkeySettings()`. Location: grep QML/`/qml/` for the existing "Settings" page entries — add a new ListView item alongside.

> The QML structure isn't visible in the recon (the QML directory wasn't found in the explore; it may be under `cpp/qml/` or `resources/qml/`). Search at execution time: `find . -name '*.qml' | xargs grep -l 'Settings' | head`.

- [ ] **Step 11.4: Build + commit**

```bash
cmake --build build-x86_64 -j 2>&1 | grep -E "error|Built target" | tail
```

```bash
git commit -m "$(cat <<'EOF'
feat(hotkeys): global libretro hotkey settings dialog

New main-settings entry point that opens GenericHotkeyPage scoped to
the libretro sentinel emuId. Reuses the existing widget — no new UI
primitives.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 12: Skip hotkey tab for libretro adapters in per-emulator dialogs

**Files:**
- Modify: `cpp/src/ui/settings/pcsx2_libretro/pcsx2_libretro_category_hub.cpp` (if exists; otherwise wherever `Pcsx2LibretroSettingsDialog` registers categories).
- Modify: equivalent mGBA libretro hub if it currently shows a hotkey tab.

- [ ] **Step 12.1: Find current hotkey-category registration**

```bash
grep -rn "HotkeyCategory\|hotkey.*addCategory\|hotkey.*Tab" cpp/src/ui/settings/
```

Inspect each match; identify the spots that register a hotkey card/tab inside a libretro-adapter dialog.

- [ ] **Step 12.2: Add the libretro skip**

At each registration site, wrap with:

```cpp
if (!adapter->asLibretro()) {
    // existing hotkey category registration
}
```

Or if the adapter accessor is named differently in that scope, use:

```cpp
if (adapter->kind() != EmulatorKind::Libretro) { ... }
```

(Whichever matches the existing API.)

- [ ] **Step 12.3: Build + visually verify**

Open the per-emulator settings for PCSX2 (libretro). Hotkey tab/card should be absent. Open DuckStation (standalone). Hotkey tab should still be present.

- [ ] **Step 12.4: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat(hotkeys): hide per-emulator hotkey tab for libretro adapters

Libretro adapters use the new global Libretro Hotkeys page instead.
Standalone adapter dialogs (DuckStation/PPSSPP/Dolphin) unchanged.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 13: Manual smoke verification

Per `[[rebuild-before-debugging-regressions]]`, **rebuild the host before testing**. Stale binaries have produced false "regressions" in this project before.

- [ ] **Step 13.1: Clean build**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
cmake --build cpp/build-x86_64 -j 2>&1 | grep -E "error|Built target" | tail
```

- [ ] **Step 13.2: PCSX2 libretro smoke (R&C 2 NTSC)**

Launch RetroNest, start R&C 2 via pcsx2-libretro. Test each binding:

- **Escape** → in-game menu opens.
- **Space** → fast-forward toggle (FF pill appears; press again, pill goes away).
- **P** → pause toggles (audio quiesces; press again to resume).
- **H** → reset (game restarts at PS2 BIOS or auto-boot).
- **F2** → save to current slot (default 1). Default slot persists.
- **F4** → load from current slot.
- **F6 / F7** → cycle save slot; OSD toast `Slot 2`, `Slot 3`...
- **Shift+F2** → save to slot 1 explicitly; **Shift+F3 → 4** likewise.
- **Shift+F7** → load slot 1; **Shift+F8 → 11** likewise.
- **M** → audio mutes; press again, audio returns.
- **+** / **-** → volume adjusts in 10% steps.

After each save, examine `<save_dir>/<serial>.slotN` (or whatever the existing save-state naming is) for the new file. Save Slot 1 ≠ Save Slot 2 on disk.

- [ ] **Step 13.3: mGBA libretro smoke**

Launch a GBA game. Verify the same hotkey list works **with zero mgba-adapter changes** — this proves the libretro-uniform claim.

Caveats expected:
- `P` (Pause) on mGBA: the `retronest_set_paused` symbol isn't exported by mgba-libretro. The fallback path in `CoreRuntime::pause()` may differ. Document outcome (pauses cleanly OR a known-graceful no-op).
- Reset works (calls `retro_reset`).
- Save/load slots work via standard `retro_serialize`/`retro_unserialize`.

- [ ] **Step 13.4: Standalone regression check**

Open DuckStation's per-emulator settings → hotkey tab is present → existing keybinds still display. No need to functionally re-test the standalone dispatch path; the goal here is to confirm no UI regression.

- [ ] **Step 13.5: Bind a gamepad combo, verify suppression**

In the new global Libretro Hotkeys page, bind `ToggleMenu` to `Gamepad0/Select+R1` (or similar). In-game:
1. Press R1 alone → game responds (R1 reaches the game).
2. Press Select alone → menu doesn't open, Select doesn't reach the game (it's a captured input).
3. Press Select+R1 → menu opens, AND game does NOT receive R1's press for this frame.

If Select-alone DOES reach the game in PS2 land, that's expected — only buttons that participate as *modifiers in a currently-matched combo* are suppressed.

- [ ] **Step 13.6: Smoke pass — write the session handoff memory**

After all checks green, write a `session_handoff_libretro_hotkeys_shipped.md` memory recording: commits, smoke-evidence, residual issues (if any), and link to the spec. Update `MEMORY.md` index.

No commit yet — the memory write goes through normal save discipline.

---

## Self-review

**Spec coverage:**
- Architecture diagram (spec §Architecture) ⇒ Tasks 1, 9, 10, 11, 12.
- Components 1 `LibretroHotkeys` ⇒ Task 1.
- Component 2 `HotkeyBindingStore` ⇒ folded into existing `ConfigService` via sentinel emuId — **deviation flagged in Task 2**.
- Component 3 `HotkeyMatcher` ⇒ Tasks 3, 4, 5.
- Component 4 Action handlers ⇒ Task 8 (dispatcher) + Task 9 (concrete callbacks).
- Component 5 `LibretroHotkeySettingsPage` ⇒ Task 11.
- Component 6 Per-emulator dialog conditional ⇒ Task 12.
- Spec data-flow "at app start" ⇒ Tasks 2 & 9 collectively.
- Spec data-flow "at game start" ⇒ Task 9.4.
- Spec data-flow "per input event" ⇒ Tasks 3–5, 9.2, 9.3.
- Spec data-flow "on settings save" ⇒ Task 9.4 (sync helper).
- Spec error-handling: conflicting bindings ⇒ Task 3/4 default (both fire); invalid JSON ⇒ ConfigService existing behavior; save-with-no-game ⇒ matcher only active during game; modifier-held suppression edge ⇒ Task 5.
- Spec testing: unit tests Tasks 1, 3, 4, 5, 6, 7, 8; integration smoke Task 13.

**Placeholder scan:** Two acknowledged areas where the plan refers to "the existing pattern" rather than spelling out exact line numbers:
- Task 2.3 — the existing shape of `hotkeyBindings()` (adapter-side data assembly): plan tells the engineer to grep and preserve existing field names.
- Task 11.3 — QML entry point location: plan tells the engineer to grep for QML directory at execution time.
- Task 12.1 — current hotkey-category registration sites: plan tells the engineer to grep.

These are NOT placeholder failures because the plan provides exact commands and tells the engineer what to look for. They reflect genuine recon gaps that require live source-code reading rather than guesswork. Each step gives the engineer enough to make the call.

**Type consistency:**
- `HotkeyMatcher::onKeyEvent(int qtKey, bool pressed)` — used consistently in Tasks 3, 9.3.
- `HotkeyMatcher::onGamepadButton(int port, int button, bool pressed)` — used in Tasks 4, 9.2.
- `HotkeyDispatcher::Callbacks` struct — defined in Task 8, populated in Task 9.1.
- `libretro_hotkeys::ids::*` namespace + `kSentinelEmuId` — used in Tasks 1, 2, 8, 11.

**Scope deviation from spec, flagged explicitly above:**
- Screenshot dropped from v1 (spec had it; recon found no existing capture path).
- `HotkeyBindingStore` collapsed into existing `ConfigService` via sentinel emuId — simpler, no new persistence class.

These are the only deviations and both are noted in the "v1 scope cuts" section near the top of this plan.
