# Setup Wizard Redesign — Design Spec

**Date:** 2026-07-10
**Status:** Approved (brainstorming) — ready for implementation planning

## Goal

Two things: (1) totally re-skin the first-run setup wizard in a new "Sunset
premium" visual system, and (2) rework its steps — drop two, add three,
and make ROM/BIOS storage locations configurable (incl. USB/external),
while scaffolding a correctly-named folder for **every** console.

The existing wizard architecture stays — `SetupWizard` QML module (separate
engine on first run), `SwipeView` + `WizardTheme` token file + per-page
components + `NavBar`. We rework the layers on top, not the skeleton.

## Visual System — "Sunset premium" (B2)

Re-skin `WizardTheme.qml` and every page to these tokens:

- **Backdrop:** full-bleed warm radial gradient
  `radial-gradient(130% 110% at 88% -12%, #ff5e8a 0%, #7a2b6b 46%, #241033 100%)`.
  Replaces the old centered bordered card — cinematic, edge-to-edge.
- **Type:** large bold titles (~48px / 800, letter-spacing −1.2px); **uppercase
  amber step labels** (`#ffd0a6`, tracked); muted-pink body (`#f2c9d8`).
- **Accent:** amber→pink gradient (`#ffb057 → #ff5e8a`) for the progress bar and
  selected states.
- **Surfaces:** translucent white "glass" fields/tiles (`#ffffff` at 8–14% +
  hairline `#ffffff2b` borders) floating on the gradient.
- **Controls:** white **pill** for the primary CTA (`#fff5f0` bg, `#3a1230`
  text); ghost text for Back; 14–16px rounded fields.
- **Spacing:** generous (≈52/72px page padding).
- **Window size:** bump the wizard window from 900×650 up to ~1180×720 (final
  values to confirm against screen size in implementation) so it feels as
  spacious as the approved mockup. Keep it a normal resizable window with sane
  minimums.
- Reference mockup: `.superpowers/brainstorm/*/content/b2-storage-page-v3.html`.

## Step Flow — 7 steps → 6

| # | Step | Change |
|---|------|--------|
| 1 | **Welcome** | Re-skin only |
| 2 | **Storage Locations** | NEW/merged — replaces old Folder + Files pages |
| 3 | **Select Emulators** | Re-skin only |
| 4 | **RetroAchievements** | NEW — log in or **Skip** |
| 5 | **ScreenScraper** | NEW — set up or **Skip** |
| 6 | **Install & Finish** | Runs install; shows ROM/BIOS folder locations |

**Removed:** Display Resolution + Aspect Ratio (they belong in Settings, not
first-run).

Ordering logic: essentials (folders) → main choice (emulators) → optional
skippable accounts → install.

### Step 2 — Storage Locations (the centerpiece)

- **Data folder** picker (path + Browse) — as today, drives the default for the
  two below.
- A collapsible **"Customize storage locations (optional)"** section (chosen
  layout: one combined step, not split) revealing:
  - **ROMs folder** — default `{data root}/roms`, Browse to anywhere (USB).
  - **BIOS folder** — default `{data root}/bios`, Browse; "point at an existing
    one and we'll use it in place."
- A note: "We'll create a correctly-named folder for **every console** — just
  drop your games in."
- `canContinue` gate: valid, absolute data-root path (reuse existing rule).

### Step 4 — RetroAchievements & Step 5 — ScreenScraper

- Each offers to authenticate now **or Skip → add later in Settings**.
- **Reuse existing mechanisms:** RA login (`RAClient`/rcheevos login) and
  scraper credential validation (`ScraperSettings` /
  `app.validateScraperCredentials`). The wizard runs in its own QML engine, so
  its backend controllers (RA login + scraper validation) must be **exposed to
  the wizard engine** — either reuse the existing objects or a thin wizard-side
  wrapper. Skipping persists nothing and leaves the Settings path untouched.

### Step 6 — Install & Finish

- Runs the install (existing `InstallController` flow, re-skinned progress).
- On completion, shows where the ROM and BIOS folders are and reminds the user
  to drop their games/BIOS in.

## Architecture — configurable ROM/BIOS roots

`Paths` currently derives everything from a single `s_root`, with `roms/` and
`bios/` hard-wired to `{root}/roms` and `{root}/bios`. Make them independent,
configurable roots — following the exact pattern `root`/`theme` already use.

**1. `Paths` gains two optional roots**
- New statics `s_romsRoot`, `s_biosRoot`, each **defaulting to `{root}/roms`
  and `{root}/bios`** when unset.
- `romsDir(systemId)` → `s_romsRoot + "/" + systemId`; `biosDir()` →
  `s_biosRoot`. These are the existing single choke points, so every consumer
  (RomScanner, adapters, `ensureRomDirectories`) inherits the change for free.
- New `setRomsRoot()` / `setBiosRoot()` apply `QDir::cleanPath` (same
  normalization we added to `setRoot`).

**2. Persistence** — `config.json` already stores `root` + `theme` via
read-modify-write helpers. Add two keys: `romsRoot`, `biosRoot`, with
`loadSaved…`/`save…` helpers mirroring the root/theme ones. **Absent/empty =
use the default** derived from root — so existing installs and "didn't
customize" both work with zero migration.

**3. Scaffold all consoles** — add `SystemRegistry::allSystemIds()` (returns
`s_entries.keys()`, mirroring the existing `allRaConsoleIds()`). On wizard
finish, create a ROM subfolder for **every** system id under the ROMs root, and
create the BIOS root.

**4. "Use existing in place" is free** — `mkpath` is idempotent, so pointing at
a USB folder that already has `PS2/`, `GBA/`, … no-ops on the existing ones and
adds any missing consoles. No copy, no move, no detection code.

**5. Clean separation** — saves/memcards/etc. stay under
`{root}/emulators/{emuId}/{systemId}/`. Relocating ROMs/BIOS to USB never moves
save data; game files can be external while app state stays local.

**6. `WizardState` wiring** — add `romsRoot` / `biosRoot` `Q_PROPERTY`s + browse
handlers (defaulting from `rootPath`, live-updating when the data folder
changes). `accept()` persists both (`saveRomsRoot`/`saveBiosRoot`) and runs the
all-systems scaffold. `main.cpp` startup also loads the saved roms/bios roots
alongside the saved root.

## Removed / Retired

- `ResolutionPage.qml`, `AspectRatioPage.qml` and their wizard wiring
  (`resolutionPage.refresh()`, `aspectRatioPage.refresh()` in `Main.qml`).
- Old `FolderPage.qml` + `FilesPage.qml` responsibilities fold into the new
  `StorageLocationsPage.qml` (delete or repurpose accordingly).

## Testing

- **Paths unit test** (extend `test_path_overrides_store` / add a paths test):
  default roms/bios roots derive from root; `setRomsRoot`/`setBiosRoot` override
  + cleanPath-normalize; empty config falls back to defaults; `romsDir`/`biosDir`
  reflect the configured roots.
- **SystemRegistry**: `allSystemIds()` returns every entry (pin count/keys).
- **Scaffold**: given a roms root, all system subfolders are created; existing
  ones are left intact (idempotent).
- **Manual/GUI**: run first-run wizard end-to-end — default path, custom USB
  path, existing-library-in-place, RA skip, RA login, scraper skip, scraper
  login, install completes, all-console folders present.

## Out of Scope (noted, not built now)

- A "change ROMs/BIOS location" control in **Settings** — the `Paths` change
  *enables* it, but it's not part of this work.
- Auto-import / copy / move / auto-sort of ROMs — explicitly rejected in favor
  of user-filed folders (the `.iso`-ambiguity problem is designed away).
- Importing existing save data.
