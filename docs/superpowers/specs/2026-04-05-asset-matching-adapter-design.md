# Move Asset Matching to Adapter Layer

## Problem

`EmulatorInstaller::matchAsset()` in `emulator_installer.cpp:179-220` contains a hardcoded if/else chain that selects the correct GitHub release asset per emulator per platform. Every new emulator requires adding a new branch to this function. The logic belongs in the adapter layer alongside all other emulator-specific behavior.

## Design

### New Virtual Method

Add to `EmulatorAdapter` base class:

```cpp
virtual QString matchAsset(const QStringList& assetNames) const;
```

**Default implementation:** Uses the generic platform-keyword fallback currently at lines 210-217 of `emulator_installer.cpp`. Matches any asset containing the platform name (`mac`/`windows`/`linux`) with a common archive extension (`.zip`, `.tar.xz`, `.dmg`, `.tar.gz`).

### Adapter Overrides

**PCSX2Adapter::matchAsset():**
- macOS: asset contains `mac` AND ends with `.tar.xz` or `.dmg`
- Windows: asset contains `windows` + `x64` AND ends with `.zip`
- Linux: asset ends with `.AppImage`

**DuckStationAdapter::matchAsset():**
- macOS: asset contains `mac` AND ends with `.zip`
- Windows: asset contains `windows` + `x64` AND ends with `.zip`
- Linux: asset contains `linux` + `x64` AND ends with `.AppImage`

These are exact transplants of the current logic from `emulator_installer.cpp` lines 191-207.

### Installer Change

Replace `EmulatorInstaller::matchAsset(emuId, assetNames)` with:

1. Look up adapter via `AdapterRegistry::instance().adapterFor(emuId)`
2. If adapter found: call `adapter->matchAsset(assetNames)`
3. If no adapter (shouldn't happen, but defensive): fall back to the generic platform-keyword matching

The `matchAsset` function on `EmulatorInstaller` changes signature from `(const QString& emuId, const QStringList& assetNames)` to just `(const QStringList& assetNames)` for the fallback, or is removed entirely if the adapter is always available at that point.

## Files Changed

| File | Change |
|------|--------|
| `cpp/src/adapters/emulator_adapter.h` | Add `virtual QString matchAsset(const QStringList&) const` with default implementation |
| `cpp/src/adapters/pcsx2_adapter.h` | Declare `matchAsset()` override |
| `cpp/src/adapters/pcsx2_adapter.cpp` | Implement with PCSX2-specific logic |
| `cpp/src/adapters/duckstation_adapter.h` | Declare `matchAsset()` override |
| `cpp/src/adapters/duckstation_adapter.cpp` | Implement with DuckStation-specific logic |
| `cpp/src/core/emulator_installer.h` | Update `matchAsset` signature or remove it |
| `cpp/src/core/emulator_installer.cpp` | Replace if/else chain with adapter call |

## Scope

- Pure refactor — no behavior change
- No new dependencies
- No UI changes
- No manifest changes
