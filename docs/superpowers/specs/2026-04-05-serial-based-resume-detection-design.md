# Serial-Based Resume State Detection

## Problem

Resume state detection relies on `resume_states.json` stored in `{config}/`. This file
maps full ROM paths to resume state file paths. It breaks in two scenarios:

1. **Reinstall + transfer saves** — the JSON is in config (not saves), so it's lost
2. **New root folder** — even if the JSON survived, full ROM paths won't match

Users who transfer their saves folder (e.g. from a USB) to a fresh install get no
resume detection, even though the resume files are physically present.

## Solution

Replace the JSON marker system with serial-based detection. Extract game serial numbers
from ROM files at import time, store them in the database, and scan savestates directories
for resume files matching each game's serial.

## Design

### Adapter Virtual Methods

Add two virtual methods to `EmulatorAdapter`:

```cpp
/// Extract game serial/ID from a ROM file (e.g. "SLUS_200.62")
virtual QString extractSerial(const QString& romPath) const { return {}; }

/// Find resume state file for a given serial in the saves directories.
/// Returns full path to the resume file, or empty string if not found.
virtual QString findResumeFile(const QString& serial, const QString& savesRoot) const { return {}; }
```

- Adapters that don't support resume return empty — no breakage
- Future emulators (GameCube, N64, PSP) implement their own ROM header parsing and
  resume file patterns

### Shared ISO9660 Helper (`core/iso9660_reader.h/cpp`)

PS1 and PS2 both use ISO9660 disc images with `SYSTEM.CNF`. A shared helper handles the
filesystem parsing so both adapters can reuse it.

**Responsibilities:**
- Read ISO9660 Primary Volume Descriptor (sector 16)
- Walk root directory to find a file by name
- Read file contents from the disc image
- Support three sector formats:
  - `.iso` — 2048-byte cooked sectors (user data only)
  - `.bin` — 2352-byte raw sectors (16-byte header + 2048 user data + 288 ECC)
  - `.chd` — compressed raw sectors via libchdr

**API:**
```cpp
namespace Iso9660 {
    /// Read a file from an ISO9660 disc image.
    /// Returns file contents, or empty QByteArray on failure.
    /// sectorSize: 2048 for .iso, 2352 for .bin
    QByteArray readFile(const QString& imagePath, const QString& filename);
}
```

The function auto-detects the sector format:
- `.chd` extension → use libchdr to read sectors
- Otherwise, check file for 2352-byte sync pattern at offset 0 (`00 FF FF...FF 00`)
  → if present, raw sectors (skip 16-byte header); otherwise 2048-byte cooked sectors

**For `.cue` files:** Parse the CUE sheet to find the data track's BIN file path,
then read from that BIN file.

**For `.m3u` files:** Read the first entry (disc 1), resolve its path, and extract
serial from that file.

### Serial Extraction (PCSX2Adapter / DuckStationAdapter)

Both adapters implement `extractSerial()` using the shared ISO9660 helper:

1. Handle `.cue` → resolve to `.bin`, `.m3u` → resolve to first disc
2. Call `Iso9660::readFile(imagePath, "SYSTEM.CNF")`
3. Parse the contents:
   - PS2: `BOOT2 = cdrom0:\SLUS_200.62;1` → extract `SLUS_200.62`
   - PS1: `BOOT = cdrom:\SCUS_941.83;1` → extract `SCUS_941.83`
4. Regex: `BOOT2?\s*=\s*cdrom0?:\\?(.+);1` → capture group 1, take filename after `\`

Since both PS1 and PS2 use the same SYSTEM.CNF format, the parsing logic can be a
shared static helper (e.g. `parseSystemCnfSerial(const QByteArray& content)`), called
by both adapters.

### Resume File Matching

Each adapter implements `findResumeFile()` with its own filename pattern:

**PCSX2Adapter:**
- Pattern: `{serial}*.resume.p2s` (e.g. `SLUS_200.62 (4F32A11F).resume.p2s`)
- Scan: `{savesRoot}/ps2/savestates/` for files starting with the serial
- The serial in the filename uses the raw format (with underscore and dot)

**DuckStationAdapter:**
- Pattern: `{serial}_resume.sav` (e.g. `SCUS_941.83_resume.sav`)  
- Scan: `{savesRoot}/psx/savestates/` for exact match
- Note: DuckStation may format the serial differently — verify at implementation time

Both methods scan only the relevant system's savestates directory (not all systems).
The adapter knows its system IDs from the manifest's `systems` array.

### Database Schema (v5)

Add a `serial` column to the `games` table:

```sql
ALTER TABLE games ADD COLUMN serial TEXT NOT NULL DEFAULT ''
```

- Populated at scan/import time by calling `adapter->extractSerial(romPath)`
- Exposed via `GameListModel` (new `SerialRole`) for debugging, but not displayed in UI
- If extraction fails (unsupported format, corrupt image), serial stays empty — resume
  detection gracefully degrades to "not available"

### Integration Points

**RomScanner (import time):**
After inserting a game into the database, call `adapter->extractSerial(romPath)` and
store the result. This happens during the existing scan loop, so no new scan pass needed.

**GameService (launch check):**
Replace `hasResumeState()`:
```
Old: read resume_states.json → check if romPath key exists → verify file exists
New: look up serial from DB → call adapter->findResumeFile(serial, savesRoot) → non-empty = has resume
```

Replace `resumeStateFile()`:
```
Old: read resume_states.json → return path value
New: same as hasResumeState but return the path
```

**GameService (save-and-quit):**
Remove the post-exit scan + JSON write logic entirely. The emulator creates the resume
file with the serial-based name — it's self-describing. No marker needed.

**GameService (clear resume state):**
`clearResumeState()` still deletes the actual resume file. Finds it via
`adapter->findResumeFile()` instead of JSON lookup.

**ThemeContext (launch with resume):**
`launchGameResume()` gets the resume file path from `adapter->findResumeFile()` instead
of `resumeStateFile()`. The `adapter->resumeLaunchArgs()` call stays the same.

### Removed Code

- `resume_states.json` — no longer created or read
- `resumeMarkerPath()` — removed
- Post-exit resume file scan + JSON write block in `GameSession::finished` handler
- `m_pendingSaveRomPath` tracking (no longer needed for marker creation; may still be
  useful for logging — evaluate at implementation time)

### Build Dependency: libchdr

Add `libchdr` for CHD file reading:
- **Method:** CMake `FetchContent` from https://github.com/rtissera/libchdr
- **License:** MIT
- **Size:** ~5 source files + zlib/lzma (commonly available)
- **Usage:** Only in `Iso9660Reader` for `.chd` sector reading

### Unsupported Formats (Graceful Degradation)

These formats won't have serial extraction initially:
- `.pbp` (PSP EBOOT format, used for PS1 on PSP) — compressed, non-standard
- `.gz` / `.cso` (compressed ISO) — would need decompression before parsing
- `.img` (varies — could be raw ISO or platform-specific)

Games in these formats will have an empty serial, and resume detection won't work for
them. This is acceptable — these are uncommon formats. Support can be added per-format
later by extending the ISO9660 reader or adding format-specific decompression.

### Flow Summary

**Normal use (no change in UX):**
```
Launch game → check serial in DB → adapter scans for resume file → show dialog if found
Save & quit → emulator writes resume file → done (no JSON update)
```

**Reinstall + transfer saves:**
```
Fresh install → import ROMs → serials extracted and stored in DB
Copy saves folder from USB → resume files are present
Launch game → serial matches resume file → resume dialog shown
```
