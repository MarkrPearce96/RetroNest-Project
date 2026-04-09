#pragma once

#include <QObject>
#include <QColor>

class SettingsTheme : public QObject {
    Q_OBJECT

    // Backgrounds
    Q_PROPERTY(QColor background READ background CONSTANT)
    Q_PROPERTY(QColor base READ base CONSTANT)
    Q_PROPERTY(QColor surface READ surface CONSTANT)
    Q_PROPERTY(QColor card READ card CONSTANT)
    Q_PROPERTY(QColor border READ border CONSTANT)

    // Accent & status
    Q_PROPERTY(QColor accent READ accent CONSTANT)
    Q_PROPERTY(QColor accentDim READ accentDim CONSTANT)
    Q_PROPERTY(QColor success READ success CONSTANT)
    Q_PROPERTY(QColor successDim READ successDim CONSTANT)
    Q_PROPERTY(QColor error READ error CONSTANT)
    Q_PROPERTY(QColor errorDim READ errorDim CONSTANT)
    Q_PROPERTY(QColor warning READ warning CONSTANT)
    Q_PROPERTY(QColor warningDim READ warningDim CONSTANT)

    // Text
    Q_PROPERTY(QColor text READ text CONSTANT)
    Q_PROPERTY(QColor textMuted READ textMuted CONSTANT)
    Q_PROPERTY(QColor textDim READ textDim CONSTANT)
    Q_PROPERTY(QColor textFaint READ textFaint CONSTANT)
    Q_PROPERTY(QColor textGhost READ textGhost CONSTANT)

    // Focus glow
    Q_PROPERTY(QColor focusBorder READ focusBorder CONSTANT)
    Q_PROPERTY(qreal focusGlowRadius READ focusGlowRadius CONSTANT)
    Q_PROPERTY(QColor focusGlow READ focusGlow CONSTANT)

    // Sizing
    Q_PROPERTY(int panelWidthPercent READ panelWidthPercent CONSTANT)
    Q_PROPERTY(int cardRadius READ cardRadius CONSTANT)
    Q_PROPERTY(int buttonRadius READ buttonRadius CONSTANT)
    Q_PROPERTY(int pillRadius READ pillRadius CONSTANT)
    Q_PROPERTY(int itemSpacing READ itemSpacing CONSTANT)
    Q_PROPERTY(int sectionSpacing READ sectionSpacing CONSTANT)

    // Animation durations (ms)
    Q_PROPERTY(int animFast READ animFast CONSTANT)
    Q_PROPERTY(int animNormal READ animNormal CONSTANT)
    Q_PROPERTY(int animSlide READ animSlide CONSTANT)

public:
    explicit SettingsTheme(QObject* parent = nullptr) : QObject(parent) {}

    // Backgrounds
    QColor background() const { return QColor("#131210"); }
    QColor base() const { return QColor("#1a1917"); }
    QColor surface() const { return QColor("#201f1c"); }
    QColor card() const { return QColor("#282621"); }
    QColor border() const { return QColor("#353330"); }

    // Accent & status
    QColor accent() const { return QColor("#e8a838"); }
    QColor accentDim() const { return QColor("#2a2518"); }
    QColor success() const { return QColor("#6a9b4a"); }
    QColor successDim() const { return QColor("#1e2a1a"); }
    QColor error() const { return QColor("#c85040"); }
    QColor errorDim() const { return QColor("#2a1a18"); }
    QColor warning() const { return QColor("#aa8844"); }
    QColor warningDim() const { return QColor("#2a2518"); }

    // Text
    QColor text() const { return QColor("#e0ddd6"); }
    QColor textMuted() const { return QColor("#8a8680"); }
    QColor textDim() const { return QColor("#6a6660"); }
    QColor textFaint() const { return QColor("#5a5650"); }
    QColor textGhost() const { return QColor("#4a4640"); }

    // Focus glow
    QColor focusBorder() const { return QColor("#e8a838"); }
    qreal focusGlowRadius() const { return 15.0; }
    QColor focusGlow() const { return QColor(232, 168, 56, 77); }  // ~0.3 alpha

    // Sizing
    int panelWidthPercent() const { return 50; }
    int cardRadius() const { return 10; }
    int buttonRadius() const { return 8; }
    int pillRadius() const { return 20; }
    int itemSpacing() const { return 8; }
    int sectionSpacing() const { return 20; }

    // Animation durations (ms)
    int animFast() const { return 150; }
    int animNormal() const { return 200; }
    int animSlide() const { return 250; }
};
