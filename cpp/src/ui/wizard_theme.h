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

    QColor background() const { return QColor("#131210"); }
    QColor surface() const { return QColor("#201f1c"); }
    QColor surfaceHover() const { return QColor("#2e2c28"); }
    QColor accent() const { return QColor("#e8a838"); }
    QColor accentLight() const { return QColor("#f0b848"); }
    QColor navBackground() const { return QColor("#1a1917"); }
    QColor cardSelected() const { return QColor("#2a2518"); }
    QColor textPrimary() const { return QColor("#e0ddd6"); }
    QColor textSecondary() const { return QColor("#c8c4b8"); }
    QColor textMuted() const { return QColor("#8a8680"); }
    QColor textDim() const { return QColor("#6a6660"); }
    QColor divider() const { return QColor("#2e2c28"); }
    QColor success() const { return QColor("#6a9b4a"); }
    QColor error() const { return QColor("#c85040"); }

    int pageMargin() const { return 48; }
    int pageTopMargin() const { return 40; }
    int cardWidth() const { return 160; }
    int cardHeight() const { return 110; }
    int cardRadius() const { return 14; }
    int cardSpacing() const { return 16; }
    int pillWidth() const { return 120; }
    int pillHeight() const { return 50; }
    int pillRadius() const { return 25; }
    int navHeight() const { return 64; }

    int animFast() const { return 150; }
    int animNormal() const { return 200; }
    int animSlow() const { return 300; }
};
