# PCSX2 Default Overrides Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Override three PCSX2 libretro default values (`pcsx2_upscale_multiplier` → 2x, `pcsx2_aspect_ratio` → 16:9, `pcsx2_enable_widescreen_patches` → enabled) via Phase E's schema-as-source-of-truth mechanism.

**Architecture:** Pure consumer of the Phase E mechanism. Six line-edits in one file, all third-positional `optLabeled(...)` / `opt(...)` arguments. No plumbing changes, no test changes, no new files.

**Tech Stack:** C++17, Qt6, CMake 3.16+.

**Spec:** `docs/superpowers/specs/2026-05-22-pcsx2-default-overrides-design.md`

---

## File Structure

One file modified. No files created. No tests added (minimal scope — see spec's "Out of scope" section for rationale).

- `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` — six 1-line edits. Each of the three target keys appears twice (Recommended-card duplicate + canonical card); both copies of each row must be updated to identical values.

---

## Task 1: Update all six PCSX2 default-value rows

**Files:**
- Modify: `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp` (six lines: 240, 258, 271, 685, 828, 895)

This task is a single commit because the three default changes are conceptually one feature (PCSX2's first-launch baseline) and don't benefit from being split — each pair of edits (canonical + Recommended) must land together to satisfy the schema's matching-defaults invariant. Splitting per-option would still mean per-pair commits, with no isolation gain.

- [ ] **Step 1: Edit `pcsx2_upscale_multiplier` — Recommended card row (line 240)**

In `cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp`, find:

```cpp
    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_upscale_multiplier", "Internal Resolution", "1",
        {{"1x Native (PS2) (Default)",     "1"},
```

Change the third positional argument of the inner row from `"1"` to `"2"`:

```cpp
    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_upscale_multiplier", "Internal Resolution", "2",
        {{"1x Native (PS2) (Default)",     "1"},
```

Only the `"1"` immediately after `"Internal Resolution"` changes. Do NOT change the inner enum entry `{"1x Native (PS2) (Default)", "1"}` — that's the option-value string for the 1x choice.

- [ ] **Step 2: Edit `pcsx2_upscale_multiplier` — canonical card row (line 895)**

Find the SECOND occurrence of `"pcsx2_upscale_multiplier"` in the same file (around line 895). It will have the same `"1"` as its third positional argument. Change it to `"2"` in the same way. Both rows must end up with `"2"` for the dupe invariant to hold.

- [ ] **Step 3: Edit `pcsx2_aspect_ratio` — Recommended card row (line 258)**

Find:

```cpp
    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_aspect_ratio", "Aspect Ratio", "4:3",
        {{"Auto (4:3 Interlaced / 3:2 Progressive)", "Auto 4:3/3:2"},
         {"4:3 (Standard)",                          "4:3"},
         {"16:9 (Widescreen)",                       "16:9"},
```

Change the third positional argument from `"4:3"` to `"16:9"`:

```cpp
    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_aspect_ratio", "Aspect Ratio", "16:9",
        {{"Auto (4:3 Interlaced / 3:2 Progressive)", "Auto 4:3/3:2"},
         {"4:3 (Standard)",                          "4:3"},
         {"16:9 (Widescreen)",                       "16:9"},
```

Only the `"4:3"` immediately after `"Aspect Ratio"` changes. The enum entry `{"4:3 (Standard)", "4:3"}` is the user-selectable 4:3 option and must stay.

- [ ] **Step 4: Edit `pcsx2_aspect_ratio` — canonical card row (line 685)**

Find the SECOND occurrence of `"pcsx2_aspect_ratio"` (around line 685). Same `"4:3"` → `"16:9"` change on its third positional argument.

- [ ] **Step 5: Edit `pcsx2_enable_widescreen_patches` — Recommended card row (line 271)**

Find:

```cpp
    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_enable_widescreen_patches", "Apply Widescreen Patches", "disabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
```

Change the third positional argument from `"disabled"` to `"enabled"`:

```cpp
    s.append(opt(
        "Recommended", "Visual Quality",
        "pcsx2_enable_widescreen_patches", "Apply Widescreen Patches", "enabled",
        {{"Enabled", "enabled"}, {"Disabled", "disabled"}},
```

Only the `"disabled"` immediately after `"Apply Widescreen Patches"` changes. The enum entry `{"Disabled", "disabled"}` must stay.

- [ ] **Step 6: Edit `pcsx2_enable_widescreen_patches` — canonical card row (line 828)**

Find the SECOND occurrence of `"pcsx2_enable_widescreen_patches"` (around line 828). Same `"disabled"` → `"enabled"` change on its third positional argument.

- [ ] **Step 7: Verify the diff is exactly six lines**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git diff cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp | grep -E "^[+-]" | grep -v "^[+-]{3}"
```

Expected output: exactly 12 lines (6 deletions + 6 additions), and every changed line shows one of:
- `-     "pcsx2_upscale_multiplier", "Internal Resolution", "1",`
- `+     "pcsx2_upscale_multiplier", "Internal Resolution", "2",`
- `-     "pcsx2_aspect_ratio", "Aspect Ratio", "4:3",`
- `+     "pcsx2_aspect_ratio", "Aspect Ratio", "16:9",`
- `-     "pcsx2_enable_widescreen_patches", "Apply Widescreen Patches", "disabled",`
- `+     "pcsx2_enable_widescreen_patches", "Apply Widescreen Patches", "enabled",`

If anything else shows up in the diff, you accidentally changed something outside the three target rows. Revert and try again.

- [ ] **Step 8: Build to verify**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build 2>&1 | tail -5
```

Expected: clean build, no errors.

- [ ] **Step 9: Run the PPSSPP schema test (sanity check the unrelated test path didn't break)**

```sh
/Users/mark/Documents/Projects/RetroNest-Project/cpp/build/test_ppsspp_libretro_schema 2>&1 | tail -3
```

Expected: `Totals: 13 passed, 0 failed, 0 skipped, 0 blacklisted, ...ms`

(This test doesn't cover PCSX2 — running it is just a "did we break Phase E itself" smoke check. We don't have a PCSX2 schema test by design — see spec.)

- [ ] **Step 10: Commit**

```sh
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/adapters/libretro/pcsx2_libretro_adapter.cpp
git commit -m "$(cat <<'EOF'
feat(pcsx2): override default resolution, aspect, widescreen patches

Sets PCSX2 first-launch defaults to better match RetroNest's target:
- pcsx2_upscale_multiplier: 1 → 2 (2x ~720p HD)
- pcsx2_aspect_ratio: 4:3 → 16:9
- pcsx2_enable_widescreen_patches: disabled → enabled

Widescreen patches are safe to enable by default because RetroNest
auto-downloads community patch files. Together with 16:9 aspect,
this gives true 16:9 viewport rendering on patch-supported games.

First consumer of the Phase E schema-as-source-of-truth mechanism
outside PPSSPP. Six line-edits in one file, no plumbing changes.

See docs/superpowers/specs/2026-05-22-pcsx2-default-overrides-design.md.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Manual verification (user action)

No subagent task. Requires GUI interaction.

- [ ] **Step 1: Reset PCSX2 options.json**

```sh
rm -f ~/Documents/RetroNest/emulators/libretro/pcsx2/options.json
ls ~/Documents/RetroNest/emulators/libretro/pcsx2/
```

Expected: `options.json` is gone (likely leaves `controls.ini` and/or `frontend.json` if they exist).

- [ ] **Step 2: Launch RetroNest**

```sh
open /Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app
```

- [ ] **Step 3: Verify the three settings show the new defaults**

In the running app: navigate Settings → PS2 → Visual Quality (or whichever card surfaces these rows; the Recommended card also surfaces them).

Confirm:
- Internal Resolution dropdown shows **"2x Native (~720px/HD)"** selected.
- Aspect Ratio dropdown shows **"16:9 (Widescreen)"** selected.
- Apply Widescreen Patches shows **"Enabled"** selected.

If any of these still show their upstream value (1x / 4:3 / Disabled), the app binary was not rebuilt by Task 1, Step 8. Re-run the build command and relaunch.

- [ ] **Step 4: Verify the persisted options.json**

```sh
cat ~/Documents/RetroNest/emulators/libretro/pcsx2/options.json | grep -E "upscale_multiplier|aspect_ratio|enable_widescreen_patches"
```

Expected output (three lines, order may vary):
```
    "pcsx2_aspect_ratio": "16:9",
    "pcsx2_enable_widescreen_patches": "enabled",
    "pcsx2_upscale_multiplier": "2",
```

- [ ] **Step 5: Launch a PS2 game and visually confirm**

Pick any PS2 ROM. Confirm:
- Crisp rendering (not blurry-pixelated like 1x native).
- 16:9 framing (no horizontal letterboxing on widescreen-patch-supported games; stretched-to-16:9 framing otherwise).

- [ ] **Step 6: Regression check — existing-value-wins**

Quit RetroNest. Manually edit options.json:

```sh
sed -i '' 's/"pcsx2_upscale_multiplier": "2"/"pcsx2_upscale_multiplier": "1"/' \
    ~/Documents/RetroNest/emulators/libretro/pcsx2/options.json
cat ~/Documents/RetroNest/emulators/libretro/pcsx2/options.json | grep upscale_multiplier
```

Expected: shows `"1"`.

Relaunch RetroNest, navigate back to Settings → PS2 → Visual Quality → Internal Resolution. Confirm dropdown shows **"1x Native (PS2) (Default)"** — proving that an existing user value beats the new schema default.

- [ ] **Step 7: Restore the new default (cleanup)**

```sh
rm ~/Documents/RetroNest/emulators/libretro/pcsx2/options.json
```

Next launch will write `"2"` fresh.

---

## Self-review notes

- Spec coverage: the three default-value changes from the spec's "In scope" table map directly to Task 1, Steps 1–6. The two-row-per-key invariant is called out in each step pair (Recommended + canonical). Manual verification (spec section "Verification → Manual") maps to Task 2.
- No placeholders. Every step shows the exact `old_string` / `new_string` content the engineer must change.
- Type consistency: there's nothing dynamic across steps — all references are to literal strings in the file.
- Diff guard (Step 7) catches the common error of changing the wrong `"1"` / `"4:3"` / `"disabled"` (e.g., touching an enum entry instead of the default).
