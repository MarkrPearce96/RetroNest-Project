#pragma once

#include <QWidget>

class AppController;
class QLabel;
class QPushButton;

/**
 * Small settings sub-page for PCSX2 patches.zip status + manual refresh.
 * Read-only status (installed / not installed) plus a "Refresh Now"
 * button that delegates to AppController::refreshPcsx2Patches().
 */
class Pcsx2PatchesPage : public QWidget {
    Q_OBJECT
public:
    Pcsx2PatchesPage(AppController* app, QWidget* parent = nullptr);

private:
    void refreshStatusLabel();

    AppController* m_app = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_button = nullptr;
};
