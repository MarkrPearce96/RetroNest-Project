# Multi-Disc M3U Support

## Problem

Multi-disc games (e.g. Final Fantasy VII, Parasite Eve) currently show as separate entries per disc in the game list. Users have no way to represent a multi-disc game as a single entry. Both PCSX2 and DuckStation natively support `.m3u` playlist files for disc switching, but the ROM scanner doesn't recognize them.

## Solution

Add M3U playlist support to the ROM scanner. When an `.m3u` file is present, it becomes the single game entry and all disc files it references are suppressed from scanning.

## Supported Layouts

Both layouts are supported — the M3U can live alongside the discs or above them:

**Subfolder layout (M3U inside):**
```
roms/psx/
  Final Fantasy VII/
    Final Fantasy VII.m3u
    Final Fantasy VII (Disc 1).chd
    Final Fantasy VII (Disc 2).chd
    Final Fantasy VII (Disc 3).chd
```

**Subfolder layout (M3U outside):**
```
roms/psx/
  Final Fantasy VII.m3u
  Final Fantasy VII/
    Final Fantasy VII (Disc 1).chd
    Final Fantasy VII (Disc 2).chd
    Final Fantasy VII (Disc 3).chd
```

**Flat layout (all in system folder):**
```
roms/psx/
  Final Fantasy VII.m3u
  Final Fantasy VII (Disc 1).chd
  Final Fantasy VII (Disc 2).chd
  Final Fantasy VII (Disc 3).chd
```

**No M3U (unchanged behavior):**
```
roms/psx/
  Final Fantasy VII (Disc 1).chd   <- shows as own game
  Final Fantasy VII (Disc 2).chd   <- shows as own game
  Final Fantasy VII (Disc 3).chd   <- shows as own game
```

## M3U File Format

Standard M3U — one file path per line, relative to the M3U file's location:
```
Final Fantasy VII (Disc 1).chd
Final Fantasy VII (Disc 2).chd
Final Fantasy VII (Disc 3).chd
```

Parsing rules:
- Skip empty lines
- Skip lines starting with `#` (comments)
- Resolve paths relative to the M3U file's parent directory
- If a path is absolute, use it as-is
- Entries referencing non-existent files are silently ignored

## Scanner Changes

### `RomScanner::scan` — two-pass approach

**Pass 1 — Collect suppression set:**
1. Iterate all files in the scan directory (recursive)
2. For each `.m3u` file found, parse it line-by-line
3. Resolve each referenced path to a canonical absolute path (`QFileInfo::canonicalFilePath`)
4. Add all resolved paths to a `QSet<QString>` (the suppression set)

**Pass 2 — Existing scan with suppression:**
1. Iterate all files as before (existing logic)
2. Before adding a file to the DB, check if its canonical path is in the suppression set
3. If suppressed, skip it (don't count as "skipped" in Result — it's intentional)
4. M3U files themselves pass through normally and get added like any ROM

The game title for an M3U entry is derived from the M3U filename, same as any other ROM file (strip extension, clean up separators).

### No changes to `scanStructured`

`scanStructured` delegates to `scan` per system folder, so it inherits M3U support automatically.

## Manifest Changes

Add `"m3u"` to `rom_extensions` in both emulator manifests:

**pcsx2.json:** `["iso", "bin", "chd", "gz", "cso", "m3u"]`

**duckstation.json:** `["bin", "cue", "iso", "img", "pbp", "chd", "m3u"]`

## Scraping

No scraper code changes needed. The scraper sends the ROM filename to ScreenScraper via the `romnom` parameter. An M3U file named `Final Fantasy VII.m3u` will match on ScreenScraper the same way any other ROM filename would — ScreenScraper matches on the game name portion of the filename.

## Launch

No launch code changes needed. Both PCSX2 and DuckStation accept `.m3u` file paths as the ROM argument and handle disc switching natively.

## Database

No schema changes. The M3U file path is stored in `rom_path` like any other ROM.

## Edge Cases

- **M3U references non-existent files:** silently ignored during suppression set building (no error, no suppression for missing files)
- **M3U with absolute paths:** resolved as-is rather than relative to M3U location
- **Multiple M3Us in same folder:** each one suppresses its own referenced files independently
- **M3U references files outside the scan directory:** suppression still works since it uses absolute paths
- **Duplicate disc across multiple M3Us:** unlikely but harmless — the disc file gets suppressed regardless of which M3U references it
