#include "pcsx2_section_header.h"
#include "../pcsx2_theme.h"
#include <QPainter>

Pcsx2SectionHeader::Pcsx2SectionHeader(const QString& text, QWidget* parent)
    : QLabel(text.toUpper(), parent) {
    setObjectName("Pcsx2SectionHeader");
    setStyleSheet(Pcsx2Theme::sectionHeaderQss());
    setContentsMargins(0, 8, 0, 4);
}

void Pcsx2SectionHeader::paintEvent(QPaintEvent* e) {
    QLabel::paintEvent(e);
    QPainter p(this);
    QColor line = Pcsx2Theme::cardBorder();
    line.setAlphaF(0.40);
    p.setPen(QPen(line, 1));
    int y = height() - 1;
    p.drawLine(0, y, width(), y);
}
