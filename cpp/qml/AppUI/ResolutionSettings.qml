import QtQuick

GenericMultiCardPicker {
    optionsLoader:  (emuId) => app.quickResolutionOptions(emuId)
    currentLoader:  (emuId) => app.currentResolution(emuId)
    applyChoices:   (choices) => app.applyQuickResolution(choices)
    optionKeyField: "value"

    previewImages: ({
        "duckstation": {
            "2": "images/res/duckstation-720p.webp",
            "3": "images/res/duckstation-1080p.webp",
            "4": "images/res/duckstation-1440p.webp",
            "6": "images/res/duckstation-4k.webp"
        }
    })
}
