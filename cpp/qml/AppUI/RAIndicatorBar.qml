import QtQuick

/**
 * RAIndicatorBar — persistent bottom-left stack of small status chips
 * driven by AppController::raIndicator(kind, data). Three chip flavours:
 *
 *   1. Challenge chips (badge only) — shown while an achievement is in
 *      "challenge mode" (e.g. mid-attempt at "without taking damage").
 *   2. Progress chips (badge + measured text like "47/100") — shown
 *      while an achievement is incrementally trackable.
 *   3. Connection chip — shown when rcheevos lost contact with the RA
 *      server. Auto-hides on reconnect after pending unlocks flush.
 *
 * `kind` integers match the rc_client_event_t enum directly so the C++
 * side just emits raw event-type values:
 *   5  = ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW
 *   6  = ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE
 *   7  = ACHIEVEMENT_PROGRESS_INDICATOR_SHOW
 *   8  = ACHIEVEMENT_PROGRESS_INDICATOR_HIDE
 *   9  = ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE
 *   17 = DISCONNECTED
 *   18 = RECONNECTED
 */
Item {
    id: root

    // Sizes itself to its content; caller anchors the Item bottom-left.
    width: column.width
    height: column.height

    property bool disconnected: false

    // Two ListModels — challenges and progress — keyed by achievement id
    // so update events can mutate in place. We can't put two roles into
    // one model and route by tag without conditional layouts in the
    // delegate, so two models keeps the visual code small.
    ListModel { id: challengeModel }
    ListModel { id: progressModel }

    function _findIdx(model, id) {
        for (var i = 0; i < model.count; ++i)
            if (model.get(i).id === id) return i;
        return -1;
    }

    /** Public entry point — invoked from AppWindow's Connections. */
    function dispatch(kind, data) {
        const id = data.id || "";
        switch (kind) {
        case 5: { // CHALLENGE_INDICATOR_SHOW
            if (_findIdx(challengeModel, id) >= 0) return;
            challengeModel.append({ id: id, title: data.title || "",
                                     badgeUrl: data.badgeUrl || "" });
            break;
        }
        case 6: { // CHALLENGE_INDICATOR_HIDE
            const i = _findIdx(challengeModel, id);
            if (i >= 0) challengeModel.remove(i);
            break;
        }
        case 7: { // PROGRESS_INDICATOR_SHOW
            const i = _findIdx(progressModel, id);
            if (i >= 0) {
                progressModel.set(i, { measured: data.measured || "" });
            } else {
                progressModel.append({ id: id, title: data.title || "",
                                        badgeUrl: data.badgeUrl || "",
                                        measured: data.measured || "" });
            }
            break;
        }
        case 8: { // PROGRESS_INDICATOR_HIDE
            const i = _findIdx(progressModel, id);
            if (i >= 0) progressModel.remove(i);
            break;
        }
        case 9: { // PROGRESS_INDICATOR_UPDATE
            const i = _findIdx(progressModel, id);
            if (i >= 0) progressModel.setProperty(i, "measured",
                                                   data.measured || "");
            break;
        }
        case 17: root.disconnected = true; break;
        case 18: root.disconnected = false; break;
        }
    }

    /** Reset on game-end so chips don't persist into the next session. */
    function clear() {
        challengeModel.clear();
        progressModel.clear();
        disconnected = false;
    }

    Column {
        id: column
        spacing: 6

        // Connection status — sits on top so it's never hidden behind
        // a tall stack of progress chips.
        Rectangle {
            visible: root.disconnected
            width: Math.max(220, label.implicitWidth + 28)
            height: 32
            radius: 16
            color: Qt.rgba(0.45, 0.05, 0.05, 0.92)   // dark red
            border.color: Qt.rgba(1, 0.4, 0.3, 0.6)
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 8
                Text {
                    text: "⚠"
                    color: "#ffd699"
                    font.pixelSize: 14
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    id: label
                    text: "RA disconnected — unlocks queued"
                    color: "#ffffff"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Challenge chips — badge only.
        Repeater {
            model: challengeModel
            delegate: Rectangle {
                width: 38
                height: 38
                radius: 10
                color: Qt.rgba(0.08, 0.08, 0.10, 0.85)
                border.color: Qt.rgba(0.95, 0.55, 0.10, 0.6) // amber
                border.width: 1
                Image {
                    anchors.fill: parent
                    anchors.margins: 3
                    source: model.badgeUrl
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                    cache: true
                }
            }
        }

        // Progress chips — badge + measured text.
        Repeater {
            model: progressModel
            delegate: Rectangle {
                width: row.implicitWidth + 18
                height: 38
                radius: 10
                color: Qt.rgba(0.08, 0.08, 0.10, 0.85)
                border.color: Qt.rgba(0.30, 0.70, 1.0, 0.45) // blue
                border.width: 1

                Row {
                    id: row
                    anchors.left: parent.left
                    anchors.leftMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Image {
                        width: 28
                        height: 28
                        source: model.badgeUrl
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        asynchronous: true
                        cache: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: model.measured
                        color: "#ffffff"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }
}
