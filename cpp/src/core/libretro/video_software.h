#pragma once

#include <QObject>
#include <QImage>
#include <atomic>

class VideoSoftware : public QObject {
    Q_OBJECT
public:
    enum class PixelFormat { RGB565, XRGB8888, ARGB1555 };
    explicit VideoSoftware(QObject* parent = nullptr);

    void setPixelFormat(PixelFormat fmt);
    void setGeometry(int baseW, int baseH, int maxW, int maxH);

    /** Called from the core thread. Copies the framebuffer into one of two
     *  internal QImages and emits frameReady() queued to the main thread. */
    void submitFrame(const void* data, int width, int height, size_t pitch);

signals:
    void frameReady(const QImage& frame);

private:
    QImage convert(const void* data, int width, int height, size_t pitch) const;

    PixelFormat m_fmt = PixelFormat::XRGB8888;
    int m_maxW = 0, m_maxH = 0;
    QImage m_buffers[2];
    std::atomic<int> m_nextBuffer{0};
};
