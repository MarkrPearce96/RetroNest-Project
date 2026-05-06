#include "libretro_video_item.h"
#include <QPainter>

LibretroVideoItem::LibretroVideoItem(QQuickItem* parent) : QQuickPaintedItem(parent) {
    setFlag(ItemHasContents);
}

void LibretroVideoItem::setFrame(const QImage& frame) {
    {
        QMutexLocker l(&m_mutex);
        m_currentFrame = frame;
    }
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void LibretroVideoItem::paint(QPainter* p) {
    QImage frame;
    {
        QMutexLocker l(&m_mutex);
        frame = m_currentFrame;
    }
    if (frame.isNull()) {
        p->fillRect(boundingRect(), Qt::black);
        return;
    }
    p->fillRect(boundingRect(), Qt::black);
    QSize fit = frame.size().scaled(boundingRect().size().toSize(), Qt::KeepAspectRatio);
    QRectF target((width() - fit.width()) / 2.0, (height() - fit.height()) / 2.0,
                  fit.width(), fit.height());
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);  // pixel-art
    p->drawImage(target, frame);
}
