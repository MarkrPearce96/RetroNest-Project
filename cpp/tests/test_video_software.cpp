#include <QtTest>
#include <QSignalSpy>
#include "core/libretro/video_software.h"

class TestVideoSoftware : public QObject {
    Q_OBJECT
private slots:
    void testRgb565ConvertsToQImage() {
        VideoSoftware vid;
        vid.setPixelFormat(VideoSoftware::PixelFormat::RGB565);
        vid.setGeometry(2, 2, 2, 2);
        QSignalSpy spy(&vid, &VideoSoftware::frameReady);
        // 2x2 RGB565: white, black, black, white
        uint16_t pixels[4] = { 0xFFFF, 0x0000, 0x0000, 0xFFFF };
        vid.submitFrame(pixels, 2, 2, 2 * sizeof(uint16_t));
        QCOMPARE(spy.count(), 1);
        QImage frame = spy.takeFirst().at(0).value<QImage>();
        QCOMPARE(frame.width(), 2);
        QCOMPARE(frame.height(), 2);
        // top-left should be white-ish (RGB888 conversion is lossy but ~255,255,255)
        QRgb tl = frame.pixel(0, 0);
        QVERIFY(qRed(tl) > 240); QVERIFY(qGreen(tl) > 240); QVERIFY(qBlue(tl) > 240);
        QRgb tr = frame.pixel(1, 0);
        QCOMPARE(qRed(tr), 0); QCOMPARE(qGreen(tr), 0); QCOMPARE(qBlue(tr), 0);
    }
    void testXrgb8888PassesThrough() {
        VideoSoftware vid;
        vid.setPixelFormat(VideoSoftware::PixelFormat::XRGB8888);
        vid.setGeometry(1, 1, 1, 1);
        QSignalSpy spy(&vid, &VideoSoftware::frameReady);
        uint32_t pixel = 0x00FF8040;  // X=00, R=FF, G=80, B=40
        vid.submitFrame(&pixel, 1, 1, sizeof(uint32_t));
        QImage frame = spy.takeFirst().at(0).value<QImage>();
        QRgb p = frame.pixel(0, 0);
        QCOMPARE(qRed(p), 0xFF); QCOMPARE(qGreen(p), 0x80); QCOMPARE(qBlue(p), 0x40);
    }
    void testGeometryChangeResizesBuffers() {
        VideoSoftware vid;
        vid.setPixelFormat(VideoSoftware::PixelFormat::RGB565);
        vid.setGeometry(4, 4, 4, 4);
        QSignalSpy spy(&vid, &VideoSoftware::frameReady);
        uint16_t pixels[16] = {};
        vid.submitFrame(pixels, 4, 4, 4 * sizeof(uint16_t));
        QImage f = spy.takeFirst().at(0).value<QImage>();
        QCOMPARE(f.width(), 4); QCOMPARE(f.height(), 4);
    }
};
QTEST_MAIN(TestVideoSoftware)
#include "test_video_software.moc"
