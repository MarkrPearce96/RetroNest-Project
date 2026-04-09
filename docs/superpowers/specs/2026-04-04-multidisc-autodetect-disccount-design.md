# Multi-Disc Auto-Detect & Disc Count

## Problem

When users add multi-disc games as loose files (e.g. `FF7 (Disc 1).chd`, `FF7 (Disc 2).chd`), each disc shows as a separate game entry. Users have to manually create M3U playlists to merge them. Additionally, there's no way to see how many discs a multi-disc game has.

## Solution

Two related enhancements to the ROM scanner:

1. **Auto-detect multi-disc groups** and silently generate M3U playlists during scanning
2. **Store and expose disc count** as metadata for M3U-based games

## Feature 1: Auto-Detect & Auto-Generate M3U

### Detection Patterns

The scanner recognizes these disc indicators in filenames (case-insensitive):

| Pattern | Example |
|---|---|
| `(Disc N)` | `Final Fantasy VII (Disc 1).chd` |
| `(Disk N)` | `Final Fantasy VII (Disk 1).chd` |
| `(CD N)` | `Final Fantasy VII (CD1).chd` |
| `(Disc N of M)` | `Final Fantasy VII (Disc 1 of 3).chd` |

A single regex covers all variants:
```
\((?:Disc|Disk|CD)\s*(\d+)(?:\s*of\s*\d+)?\)
```

### Grouping Rules

Files are grouped into a multi-disc set when:
- They share the same **base name** (filename with disc indicator stripped and trimmed)
- They have the same **file extension** (no mixing `.chd` and `.bin` in one group)
- They are in the same **directory**
- The group has **2 or more** files
- **No M3U file already exists** for that base name in the same directory

### M3U Generation

For each qualifying group:
- Create `{base name}.m3u` in the same directory as the disc files
- List disc files sorted by disc number (extracted from the pattern), one per line
- Use plain filenames (relative to the M3U's directory)

**Example:** Given `roms/psx/FF7 (Disc 1).chd`, `FF7 (Disc 2).chd`, `FF7 (Disc 3).chd`, generates `roms/psx/FF7.m3u`:
```
FF7 (Disc 1).chd
FF7 (Disc 2).chd
FF7 (Disc 3).chd
```

### Integration with Scan Flow

The auto-generation step runs **after** the BIOS directory setup and **before** the M3U suppression pass. This ordering ensures:
1. Auto-generated M3U files are picked up by the suppression pass
2. The suppression pass prevents individual disc files from being added to the DB
3. The M3U file itself is added as the single game entry

### Stale Entry Cleanup

After the main scan loop, any existing DB entries whose `rom_path` matches a file in the suppression set are removed. This handles the case where discs were previously imported individually — the old per-disc entries are cleaned up automatically when the M3U is generated on the next scan.

### What Doesn't Change

- Manually created M3U files are never overwritten or modified
- If the user doesn't have multi-disc naming conventions, nothing happens
- Single-disc games with `(Disc 1)` in the name are left alone (need 2+ to trigger)

## Feature 2: Disc Count Metadata

### Database

New column on the games table:
```sql
ALTER TABLE games ADD COLUMN disc_count INTEGER DEFAULT 0;
```

- `0` = single-disc game or non-M3U file (the default)
- `N` = number of discs (for M3U-based entries)

### Setting the Count

During `scan()`, when a file with `.m3u` extension is about to be added to the DB:
1. Parse the M3U file (reuse `parseM3u()` or count lines directly)
2. Count valid entries (non-empty, non-comment lines)
3. Set `disc_count` on the `GameRecord` before inserting

### Exposing the Count

- `GameRecord` gets a new `int disc_count = 0` field
- `Database` reads/writes the column in `addGame()`, `allGames()`, `gamesBySystem()`, `gameById()`
- `GameListModel` gets a new `DiscCountRole` mapped to `"discCount"`
- Themes access it via `model.discCount` or `themeContext.gameDetailsByIndex()` — value of `0` means single-disc (themes can choose to hide it)

## Scan Flow (Updated)

```
1. Build extension map (existing)
2. Get BIOS dir (existing)
3. ★ Auto-detect multi-disc groups → write M3U files
4. Build M3U suppression set (existing — picks up auto-generated M3Us)
5. Main file iteration with suppression (existing)
   ★ If file is .m3u → count lines → set disc_count on GameRecord
6. ★ Remove stale DB entries for now-suppressed disc files
```

## Edge Cases

- **Mixed extensions in a group** (Disc 1 is `.chd`, Disc 2 is `.bin`): not grouped — extension must match
- **Disc files in different directories**: not grouped — must share a directory
- **M3U already exists for the base name**: skip auto-generation for that group
- **Single disc with `(Disc 1)` in name**: no M3U generated (need 2+ files)
- **Rescan with existing M3U entries in DB**: disc_count gets updated if the M3U content changed
- **Auto-generated M3U file write failure** (permissions): log a warning, skip that group, discs remain as individual entries
- **Disc numbers not starting at 1** (e.g. only Disc 2 and Disc 3 present): still grouped and M3U generated — the emulator handles missing discs
- **Region tags alongside disc indicator** (e.g. `FF7 (USA) (Disc 1).chd`): the regex matches the disc indicator; the base name retains `(USA)` — so `FF7 (USA).m3u` is generated, which is correct

## Files Affected

### Modified
- `cpp/src/core/rom_scanner.h` — new `autoGenerateM3u()` helper, `removeStaleEntries()` helper
- `cpp/src/core/rom_scanner.cpp` — auto-detect logic, disc count logic, stale cleanup
- `cpp/src/core/database.h` — `disc_count` field on `GameRecord`, migration
- `cpp/src/core/database.cpp` — schema migration, read/write disc_count
- `cpp/src/ui/game_list_model.h` — `DiscCountRole`
- `cpp/src/ui/game_list_model.cpp` — expose disc_count
- `cpp/src/ui/theme_context.cpp` — include disc_count in game details
- `cpp/tests/test_rom_scanner.cpp` — new tests for auto-detect and disc count

### Not Modified
- Manifests — no changes needed (M3U already in rom_extensions)
- Scraper — M3U filename lookup works as before
- Launch — emulators accept M3U paths as before
- Theme QML — themes can optionally display disc_count but no changes required
