#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QMutex>
#include <QString>

/**
 * QQuickPaintedItem that displays a QImage stream from VideoSoftware.
 * Frames arrive on any thread via setFrame(); paint() runs on the QML
 * render thread.
 *
 * Aspect-ratio and integer-scale modes (RetroArch-style frontend control):
 *
 *   aspectMode accepted values:
 *     "native"  — preserve the frame's natural aspect (KeepAspectRatio).
 *                 Default; existing behaviour.
 *     "square"  — same as native. For libretro cores that report correct
 *                 base_width/base_height (mGBA does), the frame is already
 *                 square-pixel so "square" == "native".
 *     "4_3"     — force 4:3 display aspect; letterbox/pillarbox as needed.
 *     "16_9"    — force 16:9.
 *     "stretch" — fill the entire item rect, ignoring aspect.
 *
 *   integerScale:
 *     When true, snaps the target rect down to the largest integer multiple
 *     of the source frame size that fits. Skipped for "stretch". Eliminates
 *     pixel shimmer at the cost of some unused screen area.
 */
class LibretroVideoItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString aspectMode READ aspectMode WRITE setAspectMode NOTIFY aspectModeChanged)
    Q_PROPERTY(bool integerScale READ integerScale WRITE setIntegerScale NOTIFY integerScaleChanged)
public:
    explicit LibretroVideoItem(QQuickItem* parent = nullptr);
    void paint(QPainter* p) override;

    QString aspectMode() const { return m_aspectMode; }
    void setAspectMode(const QString& mode);

    bool integerScale() const { return m_integerScale; }
    void setIntegerScale(bool on);

public slots:
    void setFrame(const QImage& frame);

signals:
    void aspectModeChanged();
    void integerScaleChanged();

private:
    QMutex  m_mutex;
    QImage  m_currentFrame;
    QString m_aspectMode   = QStringLiteral("native");
    bool    m_integerScale = false;
};
