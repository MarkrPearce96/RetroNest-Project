#include "pcsx2_description_bar.h"
#include "../pcsx2_theme.h"
#include "core/sdl_input_manager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QFontMetrics>

Pcsx2DescriptionBar::Pcsx2DescriptionBar(QWidget* parent) : QFrame(parent) {
    setObjectName("Pcsx2DescriptionBar");
    setStyleSheet(Pcsx2Theme::descriptionBarQss());
    setMinimumHeight(100);
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    m_text = new QLabel(this);
    m_text->setObjectName("Pcsx2DescText");
    m_text->setWordWrap(true);
    m_rec = new QLabel(this);
    m_rec->setObjectName("Pcsx2DescRecommended");
    m_rec->setAlignment(Qt::AlignTop | Qt::AlignRight);
    lay->addWidget(m_text, 1);
    lay->addWidget(m_rec, 0, Qt::AlignTop);
    clear();
}

void Pcsx2DescriptionBar::setSetting(const SettingDef& def) {
    m_text->setText(def.tooltip.isEmpty()
                    ? QStringLiteral("No description available.")
                    : def.tooltip);
    QString rec = def.recommendedValue.isEmpty() ? def.defaultValue : def.recommendedValue;
    if (def.type == SettingDef::Combo) {
        for (const auto& opt : def.options) {
            if (opt.second == rec) {
                rec = opt.first;
                break;
            }
        }
    }
    m_rec->setText(QStringLiteral("Recommended: %1").arg(rec));
    m_rec->setVisible(!rec.isEmpty());
}

void Pcsx2DescriptionBar::clear() {
    m_text->setText(QStringLiteral("Focus a setting to see its description."));
    m_rec->setVisible(false);
}

void Pcsx2DescriptionBar::setHints(const QVector<ButtonHint>& hints) {
    m_hints = hints;
    setMinimumHeight(m_hints.isEmpty() ? 100 : 100);
    update();
}

void Pcsx2DescriptionBar::clearHints() {
    m_hints.clear();
    update();
}

QVector<Pcsx2DescriptionBar::ButtonHint> Pcsx2DescriptionBar::hints() const {
    return m_hints;
}

void Pcsx2DescriptionBar::setInputManager(SdlInputManager* mgr) {
    m_inputManager = mgr;  // implicit upcast SdlInputManager* → QObject* (full type visible here)
    if (mgr) {
        // String-based connect so test targets (which don't link sdl_input_manager.cpp)
        // still compile without unresolved symbol errors.
        connect(mgr, SIGNAL(controllerTypeChanged()), this, SLOT(update()));
    }
}

Pcsx2DescriptionBar::GlyphStyle Pcsx2DescriptionBar::glyphFor(const QString& action, int inputType) const {
    if (inputType == 2) {
        // PlayStation
        if (action == "confirm")     return { QStringLiteral("\u2715"), QColor("#2a3a6a"), QColor("#6d9ddc"), QColor("#3a5a8a"), 18 };
        if (action == "back")        return { QStringLiteral("\u25CB"), QColor("#5c2a3a"), QColor("#dc6d8d"), QColor("#7a3a5a"), 18 };
        if (action == "navigate_ud") return { QStringLiteral("D-Pad \u25B4\u25BE"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "navigate")    return { QStringLiteral("D-Pad \u25B4\u25BE\u25C2\u25B8"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "switch_tab")  return { QStringLiteral("L1 / R1"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    } else if (inputType == 1) {
        // Xbox
        if (action == "confirm")     return { QStringLiteral("A"), QColor("#2a5c2a"), QColor("#6ddc6d"), QColor("#3a7a3a") };
        if (action == "back")        return { QStringLiteral("B"), QColor("#5c2a2a"), QColor("#dc6d6d"), QColor("#7a3a3a") };
        if (action == "navigate_ud") return { QStringLiteral("D-Pad \u25B4\u25BE"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "navigate")    return { QStringLiteral("D-Pad \u25B4\u25BE\u25C2\u25B8"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "switch_tab")  return { QStringLiteral("LB / RB"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    } else {
        // Keyboard
        if (action == "confirm")     return { QStringLiteral("Enter"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
        if (action == "back")        return { QStringLiteral("Esc"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
        if (action == "navigate_ud") return { QStringLiteral("\u2191\u2193"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 18 };
        if (action == "navigate")    return { QStringLiteral("\u2191\u2193\u2190\u2192"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 18 };
        if (action == "switch_tab")  return { QStringLiteral("Tab"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    }
    return { QStringLiteral("?"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
}

void Pcsx2DescriptionBar::paintEvent(QPaintEvent* e) {
    QFrame::paintEvent(e);

    if (m_hints.isEmpty()) return;

    int inputType = m_inputManager ? m_inputManager->property("controllerType").toInt() : 0;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int pillHeight = 28;
    const int pillRadius = 5;
    const int pillPadX = 8;   // horizontal padding inside pill
    const int hintSpacing = 20;
    const int labelGap = 5;   // gap between pill and label

    QFont glyphFont = font();
    glyphFont.setBold(true);

    QFont labelFont = font();
    labelFont.setPixelSize(14);
    labelFont.setWeight(QFont::Medium);

    // Measure total width of all hints
    int totalWidth = 0;
    struct MeasuredHint {
        GlyphStyle glyph;
        QString label;
        int pillWidth;
        int labelWidth;
    };
    QVector<MeasuredHint> measured;
    measured.reserve(m_hints.size());

    for (const auto& h : m_hints) {
        GlyphStyle g = glyphFor(h.action, inputType);
        glyphFont.setPixelSize(g.fontSize);
        QFontMetrics gfm(glyphFont);
        int pillW = gfm.horizontalAdvance(g.text) + pillPadX * 2;

        labelFont.setPixelSize(14);
        QFontMetrics lfm(labelFont);
        int labelW = lfm.horizontalAdvance(h.label);

        measured.append({g, h.label, pillW, labelW});
        totalWidth += pillW + labelGap + labelW;
    }
    totalWidth += hintSpacing * (m_hints.size() - 1);

    // Draw hints row centered horizontally, at the bottom of the widget
    int y = height() - pillHeight - 10;
    int x = (width() - totalWidth) / 2;

    for (const auto& mh : measured) {
        // Draw pill
        QRectF pillRect(x, y, mh.pillWidth, pillHeight);
        p.setPen(QPen(mh.glyph.border, 1));
        p.setBrush(mh.glyph.bg);
        p.drawRoundedRect(pillRect, pillRadius, pillRadius);

        glyphFont.setPixelSize(mh.glyph.fontSize);
        p.setFont(glyphFont);
        p.setPen(mh.glyph.fg);
        p.drawText(pillRect, Qt::AlignCenter, mh.glyph.text);

        // Draw label
        x += mh.pillWidth + labelGap;
        labelFont.setPixelSize(14);
        p.setFont(labelFont);
        p.setPen(QColor("#dddddd"));
        QRectF labelRect(x, y, mh.labelWidth, pillHeight);
        p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, mh.label);

        x += mh.labelWidth + hintSpacing;
    }
}

QString Pcsx2DescriptionBar::descText() const { return m_text->text(); }
QString Pcsx2DescriptionBar::recommendedText() const { return m_rec->text(); }
