function logoForEmu(emuId) {
    var logos = {
        "pcsx2":       "qrc:/AppUI/qml/AppUI/images/pcsx2_logo.png",
        "duckstation": "qrc:/AppUI/qml/AppUI/images/duckstation_logo.png",
        "ppsspp":      "qrc:/AppUI/qml/AppUI/images/ppsspp_logo.png"
    }
    return logos[emuId] || ""
}
