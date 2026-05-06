#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QMutex>

/**
 * QQuickPaintedItem that displays a QImage stream from VideoSoftware.
 * Frames arrive on any thread via setFrame(); paint() runs on the QML
 * render thread.
 */
class LibretroVideoItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit LibretroVideoItem(QQuickItem* parent = nullptr);
    void paint(QPainter* p) override;

public slots:
    void setFrame(const QImage& frame);

private:
    QMutex m_mutex;
    QImage m_currentFrame;
};
