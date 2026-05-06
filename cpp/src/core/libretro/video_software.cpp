#include "video_software.h"
#include <QDebug>
#include <cstring>

VideoSoftware::VideoSoftware(QObject* parent) : QObject(parent) {}

void VideoSoftware::setPixelFormat(PixelFormat fmt) { m_fmt = fmt; }

void VideoSoftware::setGeometry(int /*baseW*/, int /*baseH*/, int /*maxW*/, int /*maxH*/) {
    // No-op: frames are converted on demand in submitFrame; no pool to pre-allocate.
}

QImage VideoSoftware::convert(const void* data, int width, int height, size_t pitch) const {
    QImage out(width, height, QImage::Format_RGB32);
    if (m_fmt == PixelFormat::XRGB8888) {
        const uint8_t* src = static_cast<const uint8_t*>(data);
        for (int y = 0; y < height; ++y) {
            std::memcpy(out.scanLine(y), src + y * pitch, width * 4);
        }
        return out;
    }
    if (m_fmt == PixelFormat::RGB565) {
        for (int y = 0; y < height; ++y) {
            const uint16_t* row = reinterpret_cast<const uint16_t*>(
                static_cast<const uint8_t*>(data) + y * pitch);
            QRgb* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
            for (int x = 0; x < width; ++x) {
                uint16_t p = row[x];
                int r = ((p >> 11) & 0x1F) * 255 / 31;
                int g = ((p >> 5)  & 0x3F) * 255 / 63;
                int b = ( p        & 0x1F) * 255 / 31;
                dst[x] = qRgb(r, g, b);
            }
        }
        return out;
    }
    // ARGB1555 — alpha bit ignored
    for (int y = 0; y < height; ++y) {
        const uint16_t* row = reinterpret_cast<const uint16_t*>(
            static_cast<const uint8_t*>(data) + y * pitch);
        QRgb* dst = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < width; ++x) {
            uint16_t p = row[x];
            int r = ((p >> 10) & 0x1F) * 255 / 31;
            int g = ((p >>  5) & 0x1F) * 255 / 31;
            int b = ( p        & 0x1F) * 255 / 31;
            dst[x] = qRgb(r, g, b);
        }
    }
    return out;
}

void VideoSoftware::submitFrame(const void* data, int width, int height, size_t pitch) {
    if (!data || width <= 0 || height <= 0) return;
    QImage frame = convert(data, width, height, pitch);
    emit frameReady(frame);
}
