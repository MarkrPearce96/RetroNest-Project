import QtQuick

GenericMultiCardPicker {
    optionsLoader:  (emuId) => app.quickAspectRatioOptions(emuId)
    currentLoader:  (emuId) => app.currentAspectRatio(emuId)
    applyChoices:   (choices) => app.applyQuickAspectRatio(choices)
    optionKeyField: "label"

    previewImages: ({
        "pcsx2":       { "4:3": "images/ar/pcsx2-4x3.webp",       "16:9": "images/ar/pcsx2-16x9.webp" },
        "duckstation": { "4:3": "images/ar/duckstation-4x3.webp", "16:9": "images/ar/duckstation-16x9.webp" }
    })
}
