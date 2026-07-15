import QtQuick

// Resolution uses the row layout (no preview image — a screenshot can't
// convey an internal-resolution scale). Each installed emulator gets its own
// row of resolution pills; picking one auto-saves.
GenericRowPicker {
    optionsLoader:  (emuId) => app.quickResolutionOptions(emuId)
    currentLoader:  (emuId) => app.currentResolution(emuId)
    applyChoices:   (choices) => app.applyQuickResolution(choices)
    optionKeyField: "value"
}
