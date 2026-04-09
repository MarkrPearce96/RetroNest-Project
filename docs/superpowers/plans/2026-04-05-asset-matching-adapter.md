# Asset Matching → Adapter Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move the hardcoded per-emulator asset matching logic from `EmulatorInstaller::matchAsset()` into the adapter layer as a virtual method, so new emulators don't require changes to the installer.

**Architecture:** Add `virtual QString matchAsset(const QStringList&) const` to `EmulatorAdapter` with a generic platform-keyword default. Override in PCSX2Adapter and DuckStationAdapter with their current specific logic. The installer's `matchAsset` becomes a thin lookup that delegates to the adapter, falling back to the generic logic if no adapter is found.

**Tech Stack:** C++17, Qt6

---

### Task 1: Add `matchAsset()` virtual method to EmulatorAdapter base class

**Files:**
- Modify: `cpp/src/adapters/emulator_adapter.h:201-211` (insert new method near other virtual methods)

- [ ] **Step 1: Add the virtual method with default implementation**

In `cpp/src/adapters/emulator_adapter.h`, add this method after `resumeLaunchArgs()` (after line 210) and before `supportsRetroAchievements()`:

```cpp
    /**
     * Select the correct GitHub release asset for this platform.
     * Override to handle emulator-specific naming conventions.
     * Default: matches any asset containing the platform name with a common archive extension.
     */
    virtual QString matchAsset(const QStringList& assetNames) const {
#if defined(Q_OS_MACOS)
        const QString platform = "mac";
#elif defined(Q_OS_WIN)
        const QString platform = "windows";
#else
        const QString platform = "linux";
#endif
        for (const auto& name : assetNames) {
            const QString lower = name.toLower();
            if (lower.contains(platform) &&
                (name.endsWith(".zip") || name.endsWith(".tar.xz") ||
                 name.endsWith(".dmg") || name.endsWith(".tar.gz"))) {
                return name;
            }
        }
        return {};
    }
```

- [ ] **Step 2: Build to verify no compile errors**

Run:
```bash
cd cpp && cmake --build build
```
Expected: Clean compile. No existing code calls this method yet, so nothing changes.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/adapters/emulator_adapter.h
git commit -m "refactor: add matchAsset() virtual method to EmulatorAdapter base class"
```

---

### Task 2: Override `matchAsset()` in PCSX2Adapter

**Files:**
- Modify: `cpp/src/adapters/pcsx2_adapter.h:37` (add declaration)
- Modify: `cpp/src/adapters/pcsx2_adapter.cpp` (add implementation at end)

- [ ] **Step 1: Declare the override in the header**

In `cpp/src/adapters/pcsx2_adapter.h`, add after the `supportsRetroAchievements()` line (line 37):

```cpp
    QString matchAsset(const QStringList& assetNames) const override;
```

- [ ] **Step 2: Implement in the .cpp file**

Add at the end of `cpp/src/adapters/pcsx2_adapter.cpp` (before the final closing, or after the last method):

```cpp
// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QString PCSX2Adapter::matchAsset(const QStringList& assetNames) const {
    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
#if defined(Q_OS_MACOS)
        if (lower.contains("mac") && (name.endsWith(".tar.xz") || name.endsWith(".dmg")))
            return name;
#elif defined(Q_OS_WIN)
        if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip"))
            return name;
#else
        if (name.endsWith(".AppImage"))
            return name;
#endif
    }
    return EmulatorAdapter::matchAsset(assetNames);
}
```

Note: Falls back to the base class generic matching if no specific match is found.

- [ ] **Step 3: Build to verify**

Run:
```bash
cd cpp && cmake --build build
```
Expected: Clean compile. The override exists but isn't called yet.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/pcsx2_adapter.h cpp/src/adapters/pcsx2_adapter.cpp
git commit -m "refactor: override matchAsset() in PCSX2Adapter"
```

---

### Task 3: Override `matchAsset()` in DuckStationAdapter

**Files:**
- Modify: `cpp/src/adapters/duckstation_adapter.h:38` (add declaration)
- Modify: `cpp/src/adapters/duckstation_adapter.cpp` (add implementation at end)

- [ ] **Step 1: Declare the override in the header**

In `cpp/src/adapters/duckstation_adapter.h`, add after the `supportsRetroAchievements()` line (line 38):

```cpp
    QString matchAsset(const QStringList& assetNames) const override;
```

- [ ] **Step 2: Implement in the .cpp file**

Add at the end of `cpp/src/adapters/duckstation_adapter.cpp`:

```cpp
// ============================================================================
// Asset matching — select the right GitHub release asset for this platform
// ============================================================================

QString DuckStationAdapter::matchAsset(const QStringList& assetNames) const {
    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
#if defined(Q_OS_MACOS)
        if (lower.contains("mac") && name.endsWith(".zip"))
            return name;
#elif defined(Q_OS_WIN)
        if (lower.contains("windows") && lower.contains("x64") && name.endsWith(".zip"))
            return name;
#else
        if (lower.contains("linux") && lower.contains("x64") && name.endsWith(".AppImage"))
            return name;
#endif
    }
    return EmulatorAdapter::matchAsset(assetNames);
}
```

- [ ] **Step 3: Build to verify**

Run:
```bash
cd cpp && cmake --build build
```
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/adapters/duckstation_adapter.h cpp/src/adapters/duckstation_adapter.cpp
git commit -m "refactor: override matchAsset() in DuckStationAdapter"
```

---

### Task 4: Update EmulatorInstaller to delegate to adapter

**Files:**
- Modify: `cpp/src/core/emulator_installer.h:45` (update signature)
- Modify: `cpp/src/core/emulator_installer.cpp:179-220` (replace if/else chain)
- Modify: `cpp/src/core/emulator_installer.cpp:1` (add include)

- [ ] **Step 1: Add adapter registry include**

At the top of `cpp/src/core/emulator_installer.cpp`, add after the existing includes (after line 19):

```cpp
#include "adapters/adapter_registry.h"
```

- [ ] **Step 2: Replace the matchAsset implementation**

Replace the entire `EmulatorInstaller::matchAsset` function (lines 179–220 of `cpp/src/core/emulator_installer.cpp`) with:

```cpp
QString EmulatorInstaller::matchAsset(const QString& emuId, const QStringList& assetNames) {
    // Delegate to the adapter if one is registered
    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    if (adapter)
        return adapter->matchAsset(assetNames);

    // Fallback: generic platform-keyword matching (no adapter registered)
    // This uses the same logic as EmulatorAdapter::matchAsset() default,
    // duplicated here because we need a static context without an adapter instance.
#if defined(Q_OS_MACOS)
    const QString platform = "mac";
#elif defined(Q_OS_WIN)
    const QString platform = "windows";
#else
    const QString platform = "linux";
#endif

    for (const auto& name : assetNames) {
        const QString lower = name.toLower();
        if (lower.contains(platform) &&
            (name.endsWith(".zip") || name.endsWith(".tar.xz") ||
             name.endsWith(".dmg") || name.endsWith(".tar.gz"))) {
            return name;
        }
    }

    return {};
}
```

Note: The signature stays the same (`static QString matchAsset(const QString& emuId, const QStringList& assetNames)`) so callers at lines 258 and 400 don't need changes. The `emuId` is now used for adapter lookup instead of if/else branching.

- [ ] **Step 3: Build to verify**

Run:
```bash
cd cpp && cmake --build build
```
Expected: Clean compile. Behavior is identical — same logic, just moved to adapters.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/emulator_installer.h cpp/src/core/emulator_installer.cpp
git commit -m "refactor: delegate asset matching to adapter, remove hardcoded if/else chain"
```

---

### Task 5: Manual verification

- [ ] **Step 1: Verify the full build is clean**

Run:
```bash
cd cpp && cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6);$(brew --prefix sdl2)" && cmake --build build
```
Expected: Clean compile with no warnings related to matchAsset.

- [ ] **Step 2: Run existing tests**

Run:
```bash
cd cpp && ctest --test-dir build --output-on-failure
```
Expected: All tests pass. (Existing tests cover IniFile and RomScanner, not installer — but confirms nothing is broken.)

- [ ] **Step 3: Verify behavior (optional, if emulators are installable)**

Launch the app and attempt to install an emulator from the emulator management page to confirm asset matching still selects the correct download. This is a pure refactor so the behavior should be identical.
