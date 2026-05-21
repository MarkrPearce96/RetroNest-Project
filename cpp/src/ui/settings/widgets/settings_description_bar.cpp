#include "settings_description_bar.h"
#include "ui/settings/settings_dialog_theme.h"
#include "core/sdl_input_manager.h"
#include "core/binding_def.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QFontMetrics>

SettingsDescriptionBar::SettingsDescriptionBar(QWidget* parent) : QFrame(parent) {
    setObjectName("SettingsDescriptionBar");
    setStyleSheet(SettingsDialogTheme::descriptionBarQss());
    setMinimumHeight(100);
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(16, 12, 16, 12);
    m_text = new QLabel(this);
    m_text->setObjectName("SettingsDescText");
    m_text->setWordWrap(true);
    m_rec = new QLabel(this);
    m_rec->setObjectName("SettingsDescRecommended");
    m_rec->setAlignment(Qt::AlignTop | Qt::AlignRight);
    m_layout->addWidget(m_text, 1);
    m_layout->addWidget(m_rec, 0, Qt::AlignTop);
    clear();
}

void SettingsDescriptionBar::setSetting(const SettingDef& def) {
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

void SettingsDescriptionBar::setHotkey(const HotkeyDef& def, const QString& currentDisplay) {
    const QString shown = currentDisplay.isEmpty()
                              ? QStringLiteral("Not bound")
                              : currentDisplay;
    m_text->setText(QStringLiteral("%1  —  Currently: %2").arg(def.label, shown));
    m_rec->setVisible(false);
}

void SettingsDescriptionBar::clear() {
    m_text->setText(QStringLiteral("Focus a setting to see its description."));
    m_rec->setVisible(false);
}

void SettingsDescriptionBar::setDescriptionVisible(bool visible) {
    m_text->setVisible(visible);
    m_rec->setVisible(visible && !m_rec->text().isEmpty());
    // When description is hidden (hub mode), shrink to just fit the hints row.
    auto m = m_layout->contentsMargins();
    m.setTop(visible ? 12 : 8);
    m.setBottom(visible ? 46 : 8);
    m_layout->setContentsMargins(m);
    setMinimumHeight(visible ? 100 : 48);
    setMaximumHeight(visible ? QWIDGETSIZE_MAX : 48);
}

void SettingsDescriptionBar::setHints(const QVector<ButtonHint>& hints) {
    m_hints = hints;
    // Reserve space at bottom for the painted hints row so text doesn't overlap.
    // Hints row: 28px pill + 10px bottom padding + 8px gap above = 46px.
    auto m = m_layout->contentsMargins();
    m.setBottom(m_hints.isEmpty() ? 12 : 46);
    m_layout->setContentsMargins(m);
    update();
}

void SettingsDescriptionBar::clearHints() {
    m_hints.clear();
    update();
}

QVector<SettingsDescriptionBar::ButtonHint> SettingsDescriptionBar::hints() const {
    return m_hints;
}

void SettingsDescriptionBar::setInputManager(SdlInputManager* mgr) {
    m_inputManager = mgr;
    if (mgr) {
        connect(mgr, SIGNAL(controllerTypeChanged()), this, SLOT(update()));
    }
}

SettingsDescriptionBar::GlyphStyle SettingsDescriptionBar::glyphFor(const QString& action, int inputType) const {
    if (inputType == 2) {
        // PlayStation
        if (action == "confirm")     return { QStringLiteral("\u2715"), QColor("#2a3a6a"), QColor("#6d9ddc"), QColor("#3a5a8a"), 18 };
        if (action == "back")        return { QStringLiteral("\u25CB"), QColor("#5c2a3a"), QColor("#dc6d8d"), QColor("#7a3a5a"), 18 };
        if (action == "clear")       return { QStringLiteral("\u25CB"), QColor("#5c2a3a"), QColor("#dc6d8d"), QColor("#7a3a5a"), 18 };  // Circle (same as back)
        if (action == "close")       return { QStringLiteral("\u25A1"), QColor("#5c2a5c"), QColor("#dc6ddc"), QColor("#7a3a7a"), 18 };  // Square
        if (action == "auto_map")    return { QStringLiteral("\u25B3"), QColor("#2a5c2a"), QColor("#6ddc6d"), QColor("#3a7a3a"), 18 };  // Triangle
        if (action == "navigate_ud") return { QStringLiteral("D-Pad \u25B4\u25BE"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "navigate")    return { QStringLiteral("D-Pad \u25B4\u25BE\u25C2\u25B8"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "switch_tab")  return { QStringLiteral("L1 / R1"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    } else if (inputType == 1) {
        // Xbox
        if (action == "confirm")     return { QStringLiteral("A"), QColor("#2a5c2a"), QColor("#6ddc6d"), QColor("#3a7a3a") };
        if (action == "back")        return { QStringLiteral("B"), QColor("#5c2a2a"), QColor("#dc6d6d"), QColor("#7a3a3a") };
        if (action == "clear")       return { QStringLiteral("B"), QColor("#5c2a2a"), QColor("#dc6d6d"), QColor("#7a3a3a") };  // same as back
        if (action == "close")       return { QStringLiteral("X"), QColor("#2a3a6a"), QColor("#6d9ddc"), QColor("#3a5a8a") };
        if (action == "auto_map")    return { QStringLiteral("Y"), QColor("#5c5a2a"), QColor("#dcd66d"), QColor("#7a7a3a") };
        if (action == "navigate_ud") return { QStringLiteral("D-Pad \u25B4\u25BE"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "navigate")    return { QStringLiteral("D-Pad \u25B4\u25BE\u25C2\u25B8"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "switch_tab")  return { QStringLiteral("LB / RB"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    } else {
        // Keyboard. Each keyboard glyph must match the actual key the dialog
        // binds to that action \u2014 clear=Del (NOT Backspace, which closes via
        // X-button/Backspace synthesis), close=Esc, etc. Don't reuse a glyph
        // across two actions or the hint will lie.
        if (action == "confirm")     return { QStringLiteral("\u21B5"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };  // \u21B5
        if (action == "back")        return { QStringLiteral("Esc"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
        if (action == "clear")       return { QStringLiteral("⌫"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 16 };
        if (action == "close")       return { QStringLiteral("Esc"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
        if (action == "auto_map")    return { QStringLiteral("M"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
        if (action == "navigate_ud") return { QStringLiteral("\u2191\u2193"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 18 };
        if (action == "navigate")    return { QStringLiteral("\u2191\u2193\u2190\u2192"), QColor("#333333"), QColor("#cccccc"), QColor("#555555"), 18 };
        if (action == "switch_tab")  return { QStringLiteral("Tab"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
    }
    return { QStringLiteral("?"), QColor("#333333"), QColor("#cccccc"), QColor("#555555") };
}

void SettingsDescriptionBar::paintEvent(QPaintEvent* e) {
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

QString SettingsDescriptionBar::descText() const { return m_text->text(); }
QString SettingsDescriptionBar::recommendedText() const { return m_rec->text(); }
