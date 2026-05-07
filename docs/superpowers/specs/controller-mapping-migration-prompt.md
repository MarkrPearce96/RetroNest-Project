# Controller Mapping Migration Prompt

Reusable prompt template for migrating an emulator (DuckStation / PPSSPP / Dolphin / future) to the schema-driven `ControllerBindingsView`. Replace `<EMULATOR_ID>`, `<CONTROLLER_TYPE_ID>`, `<CONTROLLER_DISPLAY_NAME>`, and the SVG path with values for the target emulator.

---

## Prompt

You are migrating the `<EMULATOR_ID>` emulator's controller mapping from its existing per-type binding widgets to the schema-driven `ControllerBindingsView` (already shipped for PCSX2). Working directory: `/Users/mark/Documents/Projects/RetroNest-Project`.

### Background — what already exists

- `cpp/src/ui/settings/controller_bindings_view.{h,cpp}` is a generic, reusable widget that renders a controller mapping page from a single `ControllerTypeDef` + a list of `BindingDef`s annotated with `cardSlot` + `spotlight{X,Y,R}`.
- `cpp/src/ui/settings/controller_mapping_page.cpp` is the dialog shell that hosts the view. It reads the emulator id from its constructor arg, asks the adapter for `controllerTypes()`, and embeds the view.
- The view requires **exactly one** `ControllerTypeDef` per emulator (`Q_ASSERT_X(types.size() == 1)`). Multi-type switching is intentionally not supported.
- Card slot names (used by the view's `buildSlots`) are: `"DPad"`, `"FaceButtons"`, `"LeftAnalog"`, `"RightAnalog"`, `"LeftShoulders"`, `"RightShoulders"`, `"System"`. Each slot has a fixed grid position. `"Shoulders"` is a legacy fallback at top-center for adapters that don't split.
- Spotlight `(x, y, r)` is in the SVG's intrinsic viewBox coordinate space.
- Reference implementation: see commit `01ed2f7` (PCSX2 adapter trim) and `cpp/tests/test_pcsx2_controller_schema.cpp`.

### Per-emulator inputs

| Field | Value for this migration |
|---|---|
| `<EMULATOR_ID>` | `duckstation` / `ppsspp` / `dolphin` / etc. |
| `<CONTROLLER_TYPE_ID>` | The single primary controller-type id, e.g. `"AnalogController"` for DuckStation |
| `<CONTROLLER_DISPLAY_NAME>` | Human-readable, e.g. `"Analog Controller"` |
| `<SVG_RESOURCE_PATH>` | `:/AppUI/qml/AppUI/images/controllers/<filename>.svg` |
| `<ADAPTER_FILE>` | `cpp/src/adapters/<emulator>_adapter.cpp` |
| `<ADAPTER_HEADER>` | `cpp/src/adapters/<emulator>_adapter.h` |

### Migration steps

1. **Inspect the existing adapter** to list:
   - All `ControllerTypeDef`s currently returned by `controllerTypes()`
   - The full `BindingDef` list for the primary type (the one we're keeping)
   - Any per-type setting overrides in `controllerSettingDefsForType()`

2. **Trim `<ADAPTER_FILE>` `controllerTypes()`** to a single entry:

   ```cpp
   QVector<ControllerTypeDef> <Adapter>::controllerTypes() const {
       return {
           {"<CONTROLLER_TYPE_ID>", "<CONTROLLER_DISPLAY_NAME>", "<SVG_RESOURCE_PATH>"},
       };
   }
   ```

3. **Rewrite `controllerBindingDefsForType()`** to handle only the primary type:

   ```cpp
   QVector<BindingDef> <Adapter>::controllerBindingDefsForType(const QString& type) const {
       if (type != "<CONTROLLER_TYPE_ID>") return {};
       return {
           {BindingDef::Button, "<Label>", "<Group>", "<Section>", "<Key>", "<DefaultValue>",
               "<cardSlot>", <spotlightX>, <spotlightY>, <spotlightR>},
           // ... 20-30 more, one per binding
       };
   }
   ```

   Each binding's last 4 fields:
   - `cardSlot` — must be one of: `"DPad"`, `"FaceButtons"`, `"LeftAnalog"`, `"RightAnalog"`, `"LeftShoulders"`, `"RightShoulders"`, `"System"`.
   - `spotlightX`, `spotlightY`, `spotlightR` — in the SVG viewBox. Set all three to `0` for abstract bindings (motors, pressure modifier) that don't correspond to a physical button.

   Group bindings into clusters:
   - 4 D-Pad directions → `cardSlot = "DPad"`
   - 4 Face buttons → `cardSlot = "FaceButtons"`
   - 4 Left-stick directions + L3 (if any) → `cardSlot = "LeftAnalog"`
   - 4 Right-stick directions + R3 (if any) → `cardSlot = "RightAnalog"`
   - L1, L2 → `cardSlot = "LeftShoulders"`
   - R1, R2 → `cardSlot = "RightShoulders"`
   - Select, Start, Mode/Analog, Pressure Mod, motors → `cardSlot = "System"`

4. **Read the SVG file directly** to extract real coordinates. Open the file and look for labeled elements:

   ```bash
   grep -oE 'cx="[0-9.]+" cy="[0-9.]+"|<rect[^/]*x="[0-9.]+" y="[0-9.]+"' <svg-file>
   grep -oE 'id="[A-Za-z_]+"' <svg-file> | sort -u
   ```

   Cross-reference with the buttons you're binding (D-pad cluster, sticks, face buttons, shoulders, etc.). Calibrate the `(x, y, r)` triples against the SVG's `viewBox` declaration (e.g. PCSX2's DualShock_2.svg uses `viewBox="0 0 974 664.8"`).

   Don't guess — visually-plausible-but-wrong coordinates will misalign the focus spotlight. Spot-check 2-3 of the labeled SVG elements per cluster and use the actual `cx`/`cy` numbers.

5. **Empty the per-type setting overrides**. PCSX2 dropped per-controller tuning entirely — copy that:

   ```cpp
   QVector<SettingDef> <Adapter>::controllerSettingDefs() const { return {}; }

   QVector<SettingDef> <Adapter>::controllerSettingDefsForType(const QString& type) const {
       Q_UNUSED(type);
       return {};
   }
   ```

   If the existing tunings (deadzone, sensitivity, vibration scale) feel essential, move them into the emulator's main `settingsSchema()` under a "Controller" category instead of dropping them. That's the path the spec proposed when PCSX2 considered it.

6. **Add a schema test** at `cpp/tests/test_<emulator>_controller_schema.cpp` mirroring the PCSX2 test. Use the same five test methods (rename the class):

   - `testSingleControllerType` — exactly one entry, id matches `<CONTROLLER_TYPE_ID>`, SVG path ends with the expected filename
   - `testNoControllerSettings` — both setting-defs methods return empty
   - `testBindingsHaveCardSlot` — every binding has a non-empty `cardSlot` from the valid set
   - `testPhysicalButtonsHaveSpotlight` — every button that should map to a visible artwork element has `spotlightR > 0`
   - `testNoAlternateControllerTypes` — listing the dropped types, asserting they return empty bindings + settings

   Wire the test in `cpp/CMakeLists.txt` immediately after the existing `test_<emulator>_schema` block.

7. **Verify**:

   ```bash
   cd cpp && cmake --build build && ctest --test-dir build --output-on-failure
   ```

   The new schema test passes; all 32+ existing tests still pass.

8. **Manual smoke test**:

   ```bash
   open /Users/mark/Documents/Projects/RetroNest-Project/cpp/build/RetroNest.app
   ```

   Open `<EMULATOR_ID>` controller mapping. Verify:
   - Title chrome reads `<EMULATOR_ID> — <CONTROLLER_DISPLAY_NAME>`
   - All 7 cluster slots populated with the right cards
   - Focusing each card highlights the correct physical button on the SVG
   - Pressing Cross / Enter triggers rebind on focused card
   - Pressing a controller button or keyboard key is captured and saved
   - Backspace clears the focused card; M opens / triggers Auto-Map; X / Square closes
   - Pressing Y / Triangle / M auto-maps to the active controller (toast confirms)

9. **Commit** as a single coherent change:

   ```
   <emulator>: migrate controller mapping to ControllerBindingsView

   Trim controllerTypes() to <CONTROLLER_TYPE_ID> only; rewrite
   controllerBindingDefsForType() with cardSlot + spotlight populated
   against <svg>.svg viewBox; drop per-type setting overrides; add
   test_<emulator>_controller_schema.cpp pinning the new contract.

   Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
   ```

10. **(Optional) Delete the old per-type binding widgets** if they were emulator-specific and no other adapter uses them. Don't delete anything still referenced — re-run the same `grep` audit pattern from `2026-05-07-controller-mapping-redesign-pcsx2-design.md` to confirm.

### Out of scope for the migration

- Hotkey settings page — separate dialog (`hotkey_settings_page.cpp`), not part of this work
- Multi-type switching (DigitalController vs AnalogController etc.) — view is single-type by design
- Per-port-2 — view hard-codes `port=1`; multiplayer would need a separate effort

### Reference materials

- Spec: `docs/superpowers/specs/2026-05-07-controller-mapping-redesign-pcsx2-design.md`
- PCSX2 implementation plan: `docs/superpowers/plans/2026-05-07-controller-mapping-redesign-pcsx2.md`
- View source: `cpp/src/ui/settings/controller_bindings_view.{h,cpp}`
- PCSX2 adapter (canonical example): `cpp/src/adapters/pcsx2_adapter.cpp` — `controllerTypes()` and `controllerBindingDefsForType()`
- PCSX2 schema test (template): `cpp/tests/test_pcsx2_controller_schema.cpp`
