#pragma once

#include <QObject>
#include <QColor>

class WizardTheme : public QObject {
    Q_OBJECT

    // Colors
    Q_PROPERTY(QColor background READ background CONSTANT)
    Q_PROPERTY(QColor surface READ surface CONSTANT)
    Q_PROPERTY(QColor surfaceHover READ surfaceHover CONSTANT)
    Q_PROPERTY(QColor accent READ accent CONSTANT)
    Q_PROPERTY(QColor accentLight READ accentLight CONSTANT)
    Q_PROPERTY(QColor navBackground READ navBackground CONSTANT)
    Q_PROPERTY(QColor cardSelected READ cardSelected CONSTANT)
    Q_PROPERTY(QColor textPrimary READ textPrimary CONSTANT)
    Q_PROPERTY(QColor textSecondary READ textSecondary CONSTANT)
    Q_PROPERTY(QColor textMuted READ textMuted CONSTANT)
    Q_PROPERTY(QColor textDim READ textDim CONSTANT)
    Q_PROPERTY(QColor divider READ divider CONSTANT)
    Q_PROPERTY(QColor success READ success CONSTANT)
    Q_PROPERTY(QColor error READ error CONSTANT)
    Q_PROPERTY(QColor gradTop READ gradTop CONSTANT)
    Q_PROPERTY(QColor gradMid READ gradMid CONSTANT)
    Q_PROPERTY(QColor gradBottom READ gradBottom CONSTANT)
    Q_PROPERTY(QColor surfaceBorder READ surfaceBorder CONSTANT)
    Q_PROPERTY(QColor ctaBg READ ctaBg CONSTANT)
    Q_PROPERTY(QColor ctaText READ ctaText CONSTANT)

    // Sizes
    Q_PROPERTY(int pageMargin READ pageMargin CONSTANT)
    Q_PROPERTY(int pageTopMargin READ pageTopMargin CONSTANT)
    Q_PROPERTY(int cardWidth READ cardWidth CONSTANT)
    Q_PROPERTY(int cardHeight READ cardHeight CONSTANT)
    Q_PROPERTY(int cardRadius READ cardRadius CONSTANT)
    Q_PROPERTY(int cardSpacing READ cardSpacing CONSTANT)
    Q_PROPERTY(int pillWidth READ pillWidth CONSTANT)
    Q_PROPERTY(int pillHeight READ pillHeight CONSTANT)
    Q_PROPERTY(int pillRadius READ pillRadius CONSTANT)
    Q_PROPERTY(int navHeight READ navHeight CONSTANT)

    // Animation
    Q_PROPERTY(int animFast READ animFast CONSTANT)
    Q_PROPERTY(int animNormal READ animNormal CONSTANT)
    Q_PROPERTY(int animSlow READ animSlow CONSTANT)

public:
    explicit WizardTheme(QObject* parent = nullptr) : QObject(parent) {}

    QColor background() const { return QColor("#241033"); }
    QColor surface() const { return QColor("#14ffffff"); }
    QColor surfaceHover() const { return QColor("#22ffffff"); }
    QColor accent() const { return QColor("#ff5e8a"); }
    QColor accentLight() const { return QColor("#ffb057"); }
    QColor navBackground() const { return QColor(Qt::transparent); }
    QColor cardSelected() const { return QColor("#26ffffff"); }
    QColor textPrimary() const { return QColor("#fff5f0"); }
    QColor textSecondary() const { return QColor("#f2c9d8"); }
    QColor textMuted() const { return QColor("#e7b7c7"); }
    QColor textDim() const { return QColor("#ffd0a6"); }
    QColor divider() const { return QColor("#1fffffff"); }
    QColor success() const { return QColor("#3ec6a0"); }
    QColor error() const { return QColor("#ff6b6b"); }
    QColor gradTop() const { return QColor("#ff5e8a"); }
    QColor gradMid() const { return QColor("#7a2b6b"); }
    QColor gradBottom() const { return QColor("#241033"); }
    QColor surfaceBorder() const { return QColor("#2bffffff"); }
    QColor ctaBg() const { return QColor("#fff5f0"); }
    QColor ctaText() const { return QColor("#3a1230"); }

    int pageMargin() const { return 72; }
    int pageTopMargin() const { return 52; }
    int cardWidth() const { return 160; }
    int cardHeight() const { return 110; }
    int cardRadius() const { return 14; }
    int cardSpacing() const { return 16; }
    int pillWidth() const { return 120; }
    int pillHeight() const { return 56; }
    int pillRadius() const { return 28; }
    int navHeight() const { return 64; }

    int animFast() const { return 150; }
    int animNormal() const { return 200; }
    int animSlow() const { return 300; }
};
