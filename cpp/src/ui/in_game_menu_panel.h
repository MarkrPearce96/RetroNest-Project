#pragma once

#include <QObject>
#include <QPointer>

class QQmlEngine;
class QQmlComponent;
class QQuickWindow;

/**
 * InGameMenuPanel — owns a transient QQuickWindow loaded from
 * AppUI/InGameMenuPanel.qml. The window is configured as a
 * non-activating NSPanel that floats above other apps. Show/hide
 * controls visibility; positioning is computed at show() time
 * relative to the emulator's screen.
 */
class InGameMenuPanel : public QObject {
    Q_OBJECT
public:
    explicit InGameMenuPanel(QQmlEngine* engine, QObject* parent = nullptr);
    ~InGameMenuPanel() override;

    /**
     * Show the panel positioned at the bottom-center of the screen
     * displaying the emulator with the given pid. The panel is sized
     * to a fixed HUD footprint (~480x140 px) with a 32 px bottom
     * margin from that screen.
     */
    void showOverEmulator(int64_t emulatorPid);

    /** Hide the panel (does not destroy it). */
    void hide();

    bool isVisible() const;

signals:
    void resumeRequested();
    void exitWithSaveRequested();
    void exitWithoutSaveRequested();
    void saveStateRequested();
    void loadStateRequested();
    void toggleFastForwardRequested();
    void visibilityChanged();

private:
    void ensureCreated();
    void wireSignals();
    void applyPanelChrome();

    QQmlEngine* m_engine;
    QQmlComponent* m_component = nullptr;
    QPointer<QQuickWindow> m_window;
    bool m_chromeApplied = false;
};
