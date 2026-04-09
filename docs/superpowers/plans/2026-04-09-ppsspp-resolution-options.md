# PPSSPP Resolution Options — 4-Option Set Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace PPSSPP's 11-option settings schema resolution combo and 6-option setup-wizard list with the same 4-option `720P / 1080P / 1440P / 4K` set used by PCSX2 and DuckStation, so resolution UX is consistent across all three emulators.

**Architecture:** Two in-place edits to `cpp/src/adapters/ppsspp_adapter.cpp`. Both call sites (`settingsSchema()` at line 94–100 and `resolutionOptions()` at line 576–581) read/write the same INI key (`[Graphics] InternalResolution`) and must stay in sync. No other files touched.

**Tech Stack:** C++17, Qt6, CMake. The project has no unit-test harness for adapters — verification is manual via build + launch.

**Spec:** `docs/superpowers/specs/2026-04-09-ppsspp-resolution-options-design.md`

**Mapping (multiplier → label):**

| Label | `InternalResolution` value | Actual pixels |
|-------|---------------------------|---------------|
| 720P  | `3` | 1440×816 |
| 1080P | `4` | 1920×1088 |
| 1440P | `6` | 2880×1632 |
| 4K    | `8` | 3840×2176 |

Default: `3` (720P).

---

## File Structure

- **Modify:** `cpp/src/adapters/ppsspp_adapter.cpp`
  - Line 94–100: `settingsSchema()` — `InternalResolution` combo entry
  - Line 576–581: `resolutionOptions()` — wizard option list + default

No new files. No header changes (signatures unchanged). No QML changes.

---

## Task 1: Update `settingsSchema()` resolution combo

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp:94-100`

- [ ] **Step 1: Read the current code to confirm the exact text**

Run: read `cpp/src/adapters/ppsspp_adapter.cpp` lines 94–100.
Expected to see the 11-entry combo with default `"1"`:

```cpp
    s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "1",
              {{"Auto (1:1)", "0"}, {"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"},
               {"3x (1440x816)", "3"}, {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"},
               {"6x (2880x1632)", "6"}, {"7x (3360x1904)", "7"}, {"8x (3840x2176)", "8"},
               {"9x (4320x2448)", "9"}, {"10x (4800x2720)", "10"}}, 0, 0, 0});
```

- [ ] **Step 2: Replace with the 4-option list and new default**

Edit `cpp/src/adapters/ppsspp_adapter.cpp`:

Old string:

```cpp
    s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "1",
              {{"Auto (1:1)", "0"}, {"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"},
               {"3x (1440x816)", "3"}, {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"},
               {"6x (2880x1632)", "6"}, {"7x (3360x1904)", "7"}, {"8x (3840x2176)", "8"},
               {"9x (4320x2448)", "9"}, {"10x (4800x2720)", "10"}}, 0, 0, 0});
```

New string:

```cpp
    s.append({"Graphics", "Rendering", "", "Graphics", "InternalResolution", "Rendering Resolution",
              "Rendering resolution multiplier.",
              SettingDef::Combo, "3",
              {{"720P", "3"}, {"1080P", "4"}, {"1440P", "6"}, {"4K", "8"}}, 0, 0, 0});
```

Notes:
- Default changes from `"1"` → `"3"` (720P).
- The three trailing `0, 0, 0` parameters (min/max/step for int sliders — unused for Combo) are preserved.
- Category/section/group/key (`"Graphics", "Rendering", "", "Graphics", "InternalResolution"`), label (`"Rendering Resolution"`), and description (`"Rendering resolution multiplier."`) are unchanged.

---

## Task 2: Update `resolutionOptions()` wizard list

**Files:**
- Modify: `cpp/src/adapters/ppsspp_adapter.cpp:576-581`

- [ ] **Step 1: Read the current code to confirm the exact text**

Run: read `cpp/src/adapters/ppsspp_adapter.cpp` lines 576–581.
Expected:

```cpp
ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"}, {"3x (1440x816)", "3"},
             {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"}, {"10x (4800x2720)", "10"}},
            "2"};
}
```

- [ ] **Step 2: Replace with the 4-option list and new default**

Edit `cpp/src/adapters/ppsspp_adapter.cpp`:

Old string:

```cpp
ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"1x PSP (480x272)", "1"}, {"2x (960x544)", "2"}, {"3x (1440x816)", "3"},
             {"4x (1920x1088)", "4"}, {"5x (2400x1360)", "5"}, {"10x (4800x2720)", "10"}},
            "2"};
}
```

New string:

```cpp
ResolutionOptions PPSSPPAdapter::resolutionOptions() const {
    return {"Graphics", "InternalResolution",
            {{"720P", "3"}, {"1080P", "4"}, {"1440P", "6"}, {"4K", "8"}},
            "3"};
}
```

Notes:
- Section/key (`"Graphics"`, `"InternalResolution"`) unchanged — the wizard writes the same INI key the schema reads.
- `defaultValue` changes from `"2"` → `"3"` (720P) so the wizard's pre-selection matches the schema default.
- Options list matches Task 1 exactly — this is intentional and required (both must stay in sync).

---

## Task 3: Build and verify

**Files:** none (verification only)

- [ ] **Step 1: Clean build**

Run:

```sh
cd /Users/mark/Documents/RetroNest-Project/cpp && \
  cmake --build build
```

Expected: Build succeeds. `ppsspp_adapter.cpp` recompiles. No new warnings from this file. The POST_BUILD `lsregister -f` step runs and completes without error.

If the build fails, read the error carefully — most likely causes:
- Typo in the brace-init list (mismatched `{` / `}`)
- A missing comma between option entries
- `SettingDef::Combo`/`ResolutionOptions` struct layout differs from what the old code used (should not happen — we haven't changed anything else)

Do not proceed to manual verification until the build is clean.

- [ ] **Step 2: Launch the app**

Run:

```sh
open /Users/mark/Documents/RetroNest-Project/cpp/build/RetroNest.app
```

Expected: App launches normally.

- [ ] **Step 3: Verify the Settings page combo**

Navigate: Settings → Graphics → Rendering subcategory → find "Rendering Resolution".

Expected:
- Exactly 4 options in the dropdown: `720P`, `1080P`, `1440P`, `4K`.
- No `Auto`, no `1x PSP`, no `2x`/`5x`/`6x`/`7x`/`8x`/`9x`/`10x` entries.
- If the existing `ppsspp.ini` has a stale value (e.g. `2`), the combo shows the default (720P) per the existing schema fallback — this is correct stale-value behavior.

- [ ] **Step 4: Verify the Setup Wizard page**

Trigger the setup wizard (either by deleting/renaming the config so the wizard re-runs on next launch, or by navigating to the resolution page in the existing wizard UI).

Expected:
- PPSSPP's resolution card shows exactly 4 options: `720P`, `1080P`, `1440P`, `4K`.
- `720P` is selected by default.

- [ ] **Step 5: Verify INI writes**

In the Settings page, select each option in turn and save. After each save, inspect:

```sh
cat "{root}/emulators/ppsspp/PSP/SYSTEM/ppsspp.ini" | grep InternalResolution
```

(Replace `{root}` with the user's chosen data root.)

Expected values after each selection:
- `720P`  → `InternalResolution = 3`
- `1080P` → `InternalResolution = 4`
- `1440P` → `InternalResolution = 6`
- `4K`    → `InternalResolution = 8`

If any of these don't round-trip, the option value strings don't match how PPSSPP writes the key — recheck Task 1 and Task 2.

---

## Task 4: Commit

**Files:** none (git only)

- [ ] **Step 1: Review the diff**

Run:

```sh
git diff cpp/src/adapters/ppsspp_adapter.cpp
```

Expected: Two hunks — one at `settingsSchema()` (~line 94), one at `resolutionOptions()` (~line 576). No other files touched.

- [ ] **Step 2: Stage and commit**

Run:

```sh
git add cpp/src/adapters/ppsspp_adapter.cpp
git commit -m "$(cat <<'EOF'
feat(ppsspp): unify resolution options to 720P/1080P/1440P/4K

Replace the 11-option settings combo and 6-option wizard list with the
same 4-option set used by PCSX2 and DuckStation. Maps each label to the
closest real-pixel multiplier: 720P=3x (1440x816), 1080P=4x (1920x1088),
1440P=6x (2880x1632), 4K=8x (3840x2176). Default is 720P.

Spec: docs/superpowers/specs/2026-04-09-ppsspp-resolution-options-design.md
EOF
)"
```

Expected: Commit succeeds. Pre-commit hooks (if any) pass.

- [ ] **Step 3: Verify clean tree**

Run:

```sh
git status
```

Expected: Working tree clean with respect to this change (the existing `M cpp/src/ui/app_controller.{cpp,h}` from before this task are unrelated and should remain untouched).
