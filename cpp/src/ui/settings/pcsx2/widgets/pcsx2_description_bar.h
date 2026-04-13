#pragma once
#include <QFrame>
#include <QVector>
#include "core/setting_def.h"

class QLabel;
class QHBoxLayout;
class SdlInputManager;

class Pcsx2DescriptionBar : public QFrame {
    Q_OBJECT
public:
    struct ButtonHint {
        QString action;  // "confirm", "back", "navigate_ud", "navigate", "switch_tab"
        QString label;   // "Select", "Back", "Navigate", "Switch Tab"
    };

    explicit Pcsx2DescriptionBar(QWidget* parent = nullptr);

    void setSetting(const SettingDef& def);
    void clear();

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

    QLabel* m_text = nullptr;
    QLabel* m_rec = nullptr;
    QVector<ButtonHint> m_hints;
    QObject* m_inputManager = nullptr;  // SdlInputManager*, stored as QObject* to avoid SDL headers in test targets
};
