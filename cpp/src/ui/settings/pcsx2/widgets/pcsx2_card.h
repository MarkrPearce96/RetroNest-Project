#pragma once
#include <QFrame>
#include <QEnterEvent>
#include "core/setting_def.h"

// Base card for the PCSX2 settings grid. Keyboard-focusable; repaints
// a 2 px amber halo when focused (QSS cannot draw outer glow).
class Pcsx2Card : public QFrame {
    Q_OBJECT
    Q_PROPERTY(bool focused READ hasFocus NOTIFY focusedChanged)
public:
    explicit Pcsx2Card(QWidget* parent = nullptr);
    void setSettingDef(const SettingDef& def) { m_settingDef = def; }
    void setPreviewStyle(bool preview);
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
