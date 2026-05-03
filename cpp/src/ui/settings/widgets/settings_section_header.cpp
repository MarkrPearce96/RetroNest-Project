#include "settings_section_header.h"
#include "ui/settings/settings_dialog_theme.h"
#include <QPainter>

SettingsSectionHeader::SettingsSectionHeader(const QString& text, QWidget* parent)
    : QLabel(text.toUpper(), parent) {
    setObjectName("SettingsSectionHeader");
    setStyleSheet(SettingsDialogTheme::sectionHeaderQss());
    setContentsMargins(0, 8, 0, 4);
}

void SettingsSectionHeader::paintEvent(QPaintEvent* e) {
    QLabel::paintEvent(e);
    QPainter p(this);
    QColor line = SettingsDialogTheme::cardBorder();
    line.setAlphaF(0.40);
    p.setPen(QPen(line, 1));
    int y = height() - 1;
    p.drawLine(0, y, width(), y);
}
