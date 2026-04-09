# Game Action Popup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the theme-local right-click context menu with a shared, controller-friendly game action popup accessible from all themes.

**Architecture:** A new `GameActionPopup.qml` in `qml/AppUI/` is hosted in `AppWindow.qml` as a centered modal overlay (z:150). ThemeContext gains `openGameActions(gameId)` signal and `openGameRomFolder(gameId)` method. `removeGame()` is updated to delete ROM files. Database queries sort favorites first. SdlInputManager maps Triangle/Y to `Key_M`.

**Tech Stack:** Qt6 QML, C++17, SDL2, SQLite

---

### Task 1: Add Triangle/Y → Key_M Mapping in SdlInputManager

**Files:**
- Modify: `cpp/src/core/sdl_input_manager.cpp:52-65` (mapButtonToKey function)

- [ ] **Step 1: Add the Y button mapping**

In `cpp/src/core/sdl_input_manager.cpp`, add the Y button case to `mapButtonToKey()`:

```cpp
static int mapButtonToKey(SDL_GameControllerButton btn) {
    switch (btn) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:    return Qt::Key_Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  return Qt::Key_Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  return Qt::Key_Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Qt::Key_Right;
    case SDL_CONTROLLER_BUTTON_A:          return Qt::Key_Return;
    case SDL_CONTROLLER_BUTTON_B:          return Qt::Key_Back;
    case SDL_CONTROLLER_BUTTON_X:          return Qt::Key_Backspace;
    case SDL_CONTROLLER_BUTTON_Y:          return Qt::Key_M;
    // Start handled as signal, not key injection (conflicts with Shortcuts)
    default: return 0;
    }
}
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds with no errors.

- [ ] **Step 3: Commit**

```bash
git add cpp/src/core/sdl_input_manager.cpp
git commit -m "feat: map Triangle/Y controller button to Key_M for game action popup"
```

---

### Task 2: Add Favorite-First Sort Order to Database Queries

**Files:**
- Modify: `cpp/src/core/database.cpp:253-281` (allGames and gamesBySystem SQL queries)

- [ ] **Step 1: Update allGames() query**

In `cpp/src/core/database.cpp`, change the `allGames()` SQL query at line 257 from:

```cpp
if (!q.exec(QString("SELECT %1 FROM games ORDER BY title").arg(GAME_SELECT_COLUMNS))) {
```

to:

```cpp
if (!q.exec(QString("SELECT %1 FROM games ORDER BY favorite DESC, title").arg(GAME_SELECT_COLUMNS))) {
```

- [ ] **Step 2: Update gamesBySystem() query**

In the same file, change the `gamesBySystem()` SQL query at line 271 from:

```cpp
q.prepare(QString("SELECT %1 FROM games WHERE system = ? ORDER BY title").arg(GAME_SELECT_COLUMNS));
```

to:

```cpp
q.prepare(QString("SELECT %1 FROM games WHERE system = ? ORDER BY favorite DESC, title").arg(GAME_SELECT_COLUMNS));
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/core/database.cpp
git commit -m "feat: sort favorited games to top of game lists"
```

---

### Task 3: Add openGameActions Signal and openGameRomFolder(gameId) to ThemeContext

**Files:**
- Modify: `cpp/src/ui/theme_context.h`
- Modify: `cpp/src/ui/theme_context.cpp`

- [ ] **Step 1: Add declarations to theme_context.h**

In `cpp/src/ui/theme_context.h`, add the new method and signal:

Under the "Game operations" comment (after line 40), add:

```cpp
    Q_INVOKABLE void openGameActions(int gameId);
    Q_INVOKABLE void openGameRomFolder(int gameId);
```

Under `signals:` (after line 55, the `gamesChanged` signal), add:

```cpp
    void gameActionsRequested(int gameId);
```

- [ ] **Step 2: Implement in theme_context.cpp**

In `cpp/src/ui/theme_context.cpp`, add the two new methods. Place them after the existing `openRomFolder()` method (after line 144):

```cpp
void ThemeContext::openGameActions(int gameId) {
    emit gameActionsRequested(gameId);
}

void ThemeContext::openGameRomFolder(int gameId) {
    GameRecord g = m_db->gameById(gameId);
    if (g.rom_path.isEmpty()) return;
    QString dir = QFileInfo(g.rom_path).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
```

Note: The existing no-argument `openRomFolder()` (line 140-144) stays as-is — it opens the root roms directory and is used by the empty state UI. The new `openGameRomFolder(gameId)` opens the specific ROM's containing folder.

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/theme_context.h cpp/src/ui/theme_context.cpp
git commit -m "feat: add openGameActions signal and openGameRomFolder to ThemeContext"
```

---

### Task 4: Update removeGame to Delete ROM File

**Files:**
- Modify: `cpp/src/services/game_service.h`
- Modify: `cpp/src/services/game_service.cpp`

- [ ] **Step 1: Update GameService::removeGame to accept deleteFile flag and delete ROM**

In `cpp/src/services/game_service.h`, change the method signature:

```cpp
void removeGame(int gameId, bool deleteRomFile = false);
```

In `cpp/src/services/game_service.cpp`, update the implementation (currently lines 71-73):

```cpp
void GameService::removeGame(int gameId, bool deleteRomFile) {
    if (deleteRomFile) {
        GameRecord g = m_db->gameById(gameId);
        if (!g.rom_path.isEmpty() && QFile::exists(g.rom_path)) {
            if (!QFile::remove(g.rom_path)) {
                qWarning() << "[GameService] Failed to delete ROM file:" << g.rom_path;
            }
        }
    }
    m_db->removeGame(gameId);
}
```

Add at the top of `game_service.cpp` if not already present:

```cpp
#include <QFile>
```

- [ ] **Step 2: Update AppController::removeGame to pass deleteRomFile=true**

In `cpp/src/ui/app_controller.cpp`, update `removeGame()` (line 134-139):

```cpp
void AppController::removeGame(int gameId) {
    m_gameService.removeGame(gameId, true);
    setStatus("Game removed.");
    emit systemsChanged();
    emit gamesChanged();
}
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/src/services/game_service.h cpp/src/services/game_service.cpp cpp/src/ui/app_controller.cpp
git commit -m "feat: delete ROM file from disk when removing game from library"
```

---

### Task 5: Create GameActionPopup.qml

**Files:**
- Create: `cpp/qml/AppUI/GameActionPopup.qml`

- [ ] **Step 1: Create the popup component**

Create `cpp/qml/AppUI/GameActionPopup.qml`:

```qml
import QtQuick
import QtQuick.Controls

Item {
    id: popup
    anchors.fill: parent
    visible: false
    z: 150

    property int targetGameId: -1
    property bool isFavorite: false
    property string gameTitle: ""

    // State: "actions" shows action list, "confirm" shows delete confirmation
    property string popupState: "actions"
    property int focusIndex: 0

    function open(gameId) {
        var details = themeContext.gameDetails(gameId)
        targetGameId = gameId
        isFavorite = details.favorite === 1
        gameTitle = details.title
        popupState = "actions"
        focusIndex = 0
        visible = true
        popup.forceActiveFocus()
    }

    function close() {
        visible = false
        targetGameId = -1
    }

    // Dark scrim
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.7)

        MouseArea {
            anchors.fill: parent
            onClicked: popup.close()
        }
    }

    // Centered card
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 400
        height: popupState === "actions" ? actionColumn.height + 32 : confirmColumn.height + 32
        radius: 12
        color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
        border.color: Qt.rgba(1, 1, 1, 0.1)
        border.width: 1

        Behavior on height { NumberAnimation { duration: 150 } }

        // Title
        Text {
            id: titleText
            anchors.top: parent.top
            anchors.topMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter
            text: popup.gameTitle
            color: Qt.rgba(1, 1, 1, 0.6)
            font.pixelSize: 13
            elide: Text.ElideRight
            width: parent.width - 32
            horizontalAlignment: Text.AlignHCenter
        }

        // ── Action List ──
        Column {
            id: actionColumn
            anchors.top: titleText.bottom
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 32
            spacing: 2
            visible: popupState === "actions"

            Repeater {
                model: [
                    { label: "Scrape", actionId: "scrape", destructive: false },
                    { label: popup.isFavorite ? "Remove from Favorites" : "Add to Favorites",
                      actionId: "favorite", destructive: false },
                    { label: "Open ROM Folder", actionId: "openFolder", destructive: false },
                    { label: "Remove from Library", actionId: "remove", destructive: true }
                ]

                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: actionColumn.width
                    height: 44
                    radius: 6
                    color: popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: modelData.destructive ? "#ff4444" :
                               (popup.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.7))
                        font.pixelSize: 16
                        font.weight: popup.focusIndex === index ? Font.DemiBold : Font.Normal
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: popup.executeAction(modelData.actionId)
                    }
                }
            }
        }

        // ── Confirm Delete ──
        Column {
            id: confirmColumn
            anchors.top: titleText.bottom
            anchors.topMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 32
            spacing: 12
            visible: popupState === "confirm"

            Text {
                width: parent.width
                text: "Delete ROM and remove from library?"
                color: "#ff4444"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 16

                Repeater {
                    model: ["Yes", "No"]

                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        width: 120
                        height: 44
                        radius: 6
                        color: popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                        border.color: popup.focusIndex === index ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: index === 0 ? "#ff4444" :
                                   (popup.focusIndex === index ? "#ffffff" : Qt.rgba(1, 1, 1, 0.7))
                            font.pixelSize: 16
                            font.weight: popup.focusIndex === index ? Font.DemiBold : Font.Normal
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (index === 0) popup.confirmRemove()
                                else popup.cancelRemove()
                            }
                        }
                    }
                }
            }
        }
    }

    function executeAction(actionId) {
        switch (actionId) {
        case "scrape":
            themeContext.scrapeGame(targetGameId)
            close()
            break
        case "favorite":
            themeContext.toggleFavorite(targetGameId)
            isFavorite = !isFavorite
            break
        case "openFolder":
            themeContext.openGameRomFolder(targetGameId)
            close()
            break
        case "remove":
            popupState = "confirm"
            focusIndex = 1  // Default to "No"
            break
        }
    }

    function confirmRemove() {
        themeContext.removeGame(targetGameId)
        close()
    }

    function cancelRemove() {
        popupState = "actions"
        focusIndex = 0
    }

    Keys.onPressed: function(event) {
        if (!visible) return

        if (popupState === "actions") {
            var actionCount = 4
            if (event.key === Qt.Key_Up) {
                focusIndex = (focusIndex - 1 + actionCount) % actionCount
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                focusIndex = (focusIndex + 1) % actionCount
                event.accepted = true
            } else if (event.key === Qt.Key_Return) {
                var actions = ["scrape", "favorite", "openFolder", "remove"]
                executeAction(actions[focusIndex])
                event.accepted = true
            } else if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape ||
                       event.key === Qt.Key_M) {
                close()
                event.accepted = true
            }
        } else if (popupState === "confirm") {
            if (event.key === Qt.Key_Left) {
                focusIndex = 0
                event.accepted = true
            } else if (event.key === Qt.Key_Right) {
                focusIndex = 1
                event.accepted = true
            } else if (event.key === Qt.Key_Return) {
                if (focusIndex === 0) confirmRemove()
                else cancelRemove()
                event.accepted = true
            } else if (event.key === Qt.Key_Back || event.key === Qt.Key_Escape) {
                cancelRemove()
                event.accepted = true
            }
        }
    }
}
```

- [ ] **Step 2: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds (component not yet referenced, just needs to parse).

- [ ] **Step 3: Commit**

```bash
git add cpp/qml/AppUI/GameActionPopup.qml
git commit -m "feat: create GameActionPopup shared overlay component"
```

---

### Task 6: Host GameActionPopup in AppWindow and Register in CMake

**Files:**
- Modify: `cpp/qml/AppUI/AppWindow.qml`
- Modify: `cpp/CMakeLists.txt`

- [ ] **Step 1: Add GameActionPopup to CMakeLists.txt**

In `cpp/CMakeLists.txt`, add the new file to the QML_FILES section (after line 179, the ProgressPopup.qml entry):

```
        qml/AppUI/GameActionPopup.qml
```

- [ ] **Step 2: Add GameActionPopup to AppWindow.qml**

In `cpp/qml/AppUI/AppWindow.qml`, add the popup instance and its connections. After the SettingsOverlay block (after line 73), add:

```qml
    // Game action popup (M key / Triangle button)
    GameActionPopup {
        id: gameActionPopup
    }

    Connections {
        target: themeContext
        function onGameActionsRequested(gameId) {
            gameActionPopup.open(gameId)
        }
    }
```

- [ ] **Step 3: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add cpp/qml/AppUI/AppWindow.qml cpp/CMakeLists.txt
git commit -m "feat: host GameActionPopup in AppWindow and register in CMake"
```

---

### Task 7: Migrate Modern Theme — Remove Context Menu, Add Key_M Handler and Favorite Star

**Files:**
- Modify: `themes/modern/GameListPage.qml`

- [ ] **Step 1: Add `favorite` required property to the delegate**

In `themes/modern/GameListPage.qml`, in the delegate (around line 579, after `required property int index`), add:

```qml
                required property bool favorite
```

- [ ] **Step 2: Add star icon to the delegate**

In the delegate, after the title Text element (after line 605), add a star indicator:

```qml
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    text: "\u2605"
                    color: "#ffc107"
                    font.pixelSize: 18
                    visible: listItem.favorite
                }
```

Also update the title Text's `anchors.rightMargin` to `36` (from `14`) to make space for the star:

Change line 599 from:
```qml
                    anchors.rightMargin: 14
```
to:
```qml
                    anchors.rightMargin: 36
```

- [ ] **Step 3: Add Key_M handler to the game list**

In `GameListPage.qml`, find the `Keys.onPressed` handler for the game list (or the root item's key handling section). Add a `Key_M` handler that opens the popup for the currently highlighted game. This should be added in the main `Keys.onPressed` handler for the page:

```qml
            } else if (event.key === Qt.Key_M) {
                if (gameModel.count > 0) {
                    var details = themeContext.gameDetailsByIndex(root.listIndex)
                    if (details.id)
                        themeContext.openGameActions(details.id)
                }
                event.accepted = true
```

- [ ] **Step 4: Remove the right-click context menu**

Delete the entire context menu section (lines 629-661):

```qml
    // ════════════════════════════════════════════════════════════════
    // CONTEXT MENU
    // ════════════════════════════════════════════════════════════════
    Menu {
        id: contextMenu
        ...
    }
```

Also remove the right-click handling from the MouseArea in the delegate (around lines 607-624). Change the MouseArea to only handle left-click:

```qml
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        root.listIndex = listItem.index
                        root.selectCurrentGame()
                    }
                    onDoubleClicked: {
                        themeContext.launchGame(listItem.gameId, listItem.romPath, listItem.emulatorId)
                    }
                }
```

- [ ] **Step 5: Build and verify**

Run:
```bash
cd cpp && cmake --build build 2>&1 | tail -5
```
Expected: Build succeeds.

- [ ] **Step 6: Manual test**

1. Launch the app
2. Navigate to a game list
3. Press M (keyboard) or Triangle/Y (controller) — popup should appear
4. Navigate with arrow keys, confirm actions work (scrape, favorite, open folder)
5. Favorite a game — star appears, game moves to top
6. Test remove — confirmation dialog appears, default on "No"
7. Verify right-click no longer shows a native menu

- [ ] **Step 7: Commit**

```bash
git add themes/modern/GameListPage.qml
git commit -m "feat: migrate modern theme to use shared game action popup with favorite stars"
```
