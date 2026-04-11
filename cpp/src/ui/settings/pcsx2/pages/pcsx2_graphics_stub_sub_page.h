#pragma once
#include <QWidget>

class AppController;

// Placeholder sub-page used for Graphics/Display and Graphics/OSD in
// Plan 2. Plans 3 and 4 replace these with real pages. Shows a
// "Coming in a later update" label and a button that opens the legacy
// EmulatorSettingsPage as a modal escape hatch.
class Pcsx2GraphicsStubSubPage : public QWidget {
    Q_OBJECT
public:
    Pcsx2GraphicsStubSubPage(AppController* app,
                             const QString& emuId,
                             const QString& subTabName,
                             QWidget* parent = nullptr);

private slots:
    void openLegacyDialog();

private:
    AppController* m_app;
    QString m_emuId;
    QString m_subTabName;
};
