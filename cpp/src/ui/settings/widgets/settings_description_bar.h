#pragma once
#include <QFrame>
#include <QVector>
#include "core/setting_def.h"

struct HotkeyDef;

class QLabel;
class QHBoxLayout;
class SdlInputManager;

class SettingsDescriptionBar : public QFrame {
    Q_OBJECT
public:
    struct ButtonHint {
        QString action;  // "confirm", "back", "navigate_ud", "navigate", "switch_tab"
        QString label;   // "Select", "Back", "Navigate", "Switch Tab"
    };

    explicit SettingsDescriptionBar(QWidget* parent = nullptr);

    void setSetting(const SettingDef& def);
    void clear();
    void setDescriptionVisible(bool visible);

    // Hotkey variant: writes "<Label> — Currently: <value or 'Not bound'>"
    // to the primary text and hides the recommended badge.
    void setHotkey(const HotkeyDef& def, const QString& currentDisplay);

    void setHints(const QVector<ButtonHint>& hints);
    void clearHints();
    QVector<ButtonHint> hints() const;

    void setInputManager(SdlInputManager* mgr);

    // test hooks
    QString descText() const;
    QString recommendedText() const;

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    struct GlyphStyle {
        QString text;
        QColor bg;
        QColor fg;
        QColor border;
        int fontSize = 14;
    };
    GlyphStyle glyphFor(const QString& action, int inputType) const;

    QHBoxLayout* m_layout = nullptr;
    QLabel* m_text = nullptr;
    QLabel* m_rec = nullptr;
    QVector<ButtonHint> m_hints;
    QObject* m_inputManager = nullptr;
};
