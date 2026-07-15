import QtQuick

GenericMultiCardPicker {
    autoSave:       true
    optionsLoader:  (emuId) => app.quickAspectRatioOptions(emuId)
    currentLoader:  (emuId) => app.currentAspectRatio(emuId)
    applyChoices:   (choices) => app.applyQuickAspectRatio(choices)
    // Core aspect options store their value == label (e.g. "4:3", "16:9"), so
    // key on "value" like Resolution; the preview map matches those values.
    optionKeyField: "value"

    previewImages: ({
        "pcsx2":       { "4:3": "images/ar/pcsx2-4x3.webp",       "16:9": "images/ar/pcsx2-16x9.webp" },
        "duckstation": { "4:3": "images/ar/duckstation-4x3.webp", "16:9": "images/ar/duckstation-16x9.webp" }
    })
}
