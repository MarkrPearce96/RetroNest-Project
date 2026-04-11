#include "pcsx2_description_bar.h"
#include "../pcsx2_theme.h"
#include <QHBoxLayout>
#include <QLabel>

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
    const QString rec = def.recommendedValue.isEmpty() ? def.defaultValue : def.recommendedValue;
    m_rec->setText(QStringLiteral("Recommended: %1").arg(rec));
    m_rec->setVisible(!rec.isEmpty());
}

void Pcsx2DescriptionBar::clear() {
    m_text->setText(QStringLiteral("Focus a setting to see its description."));
    m_rec->setVisible(false);
}

QString Pcsx2DescriptionBar::descText() const { return m_text->text(); }
QString Pcsx2DescriptionBar::recommendedText() const { return m_rec->text(); }
