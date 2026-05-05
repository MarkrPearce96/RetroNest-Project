#pragma once
#include <QFrame>
#include <QEnterEvent>
#include "core/setting_def.h"

// Base card for the PCSX2 settings grid. Keyboard-focusable; repaints
// a 2 px amber halo when focused (QSS cannot draw outer glow).
class SettingsCard : public QFrame {
    Q_OBJECT
    Q_PROPERTY(bool focused READ hasFocus NOTIFY focusedChanged)
public:
    explicit SettingsCard(QWidget* parent = nullptr);
    void setSettingDef(const SettingDef& def) { m_settingDef = def; }
    void setPreviewStyle(bool preview);
    // Pin this card to the uniform reference height (computed once at
    // runtime from a SettingsComboRow's natural rendered size). Page-
    // builder calls this for combo/slider/toggle cards so they line up
    // across pages with mixed content. Hub category cards and preview
    // cards skip this and set their own heights.
    void pinToReferenceHeight();
    const SettingDef& settingDef() const { return m_settingDef; }

signals:
    void focused(SettingDef def);
    void focusedChanged();
    void activated(); // Enter / Return

protected:
    void focusInEvent(QFocusEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;

private:
    SettingDef m_settingDef;
};
