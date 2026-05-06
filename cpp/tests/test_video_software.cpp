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
    void testArgb1555ConvertsToQImage() {
        VideoSoftware vid;
        vid.setPixelFormat(VideoSoftware::PixelFormat::ARGB1555);
        vid.setGeometry(2, 1, 2, 1);
        QSignalSpy spy(&vid, &VideoSoftware::frameReady);
        // 2x1 ARGB1555: 0b0_11111_11111_11111 (white, alpha=0), 0b0_10000_00000_00000 (~half red)
        uint16_t pixels[2] = { 0x7FFF, 0x4000 };
        vid.submitFrame(pixels, 2, 1, 2 * sizeof(uint16_t));
        QImage f = spy.takeFirst().at(0).value<QImage>();
        QRgb white = f.pixel(0, 0);
        QVERIFY(qRed(white) > 240); QVERIFY(qGreen(white) > 240); QVERIFY(qBlue(white) > 240);
        QRgb halfRed = f.pixel(1, 0);
        // 0x10 in 5 bits → 16/31 ≈ 50% red
        QVERIFY(qRed(halfRed) > 110 && qRed(halfRed) < 145);
        QCOMPARE(qGreen(halfRed), 0); QCOMPARE(qBlue(halfRed), 0);
    }
    void testNonTrivialPitchHandled() {
        VideoSoftware vid;
        vid.setPixelFormat(VideoSoftware::PixelFormat::RGB565);
        vid.setGeometry(2, 2, 2, 2);
        QSignalSpy spy(&vid, &VideoSoftware::frameReady);
        // 2x2 frame with stride of 4 pixels (extra padding ignored)
        uint16_t pixels[8] = {
            0xFFFF, 0x0000, 0xDEAD, 0xDEAD,   // row 0: white, black, garbage, garbage
            0x0000, 0xFFFF, 0xBEEF, 0xBEEF,   // row 1: black, white, garbage, garbage
        };
        vid.submitFrame(pixels, 2, 2, 4 * sizeof(uint16_t));   // pitch = 8 bytes per row
        QImage f = spy.takeFirst().at(0).value<QImage>();
        QCOMPARE(f.width(), 2); QCOMPARE(f.height(), 2);
        QRgb tl = f.pixel(0, 0);
        QVERIFY(qRed(tl) > 240);   // top-left = white from row 0[0]
        QRgb br = f.pixel(1, 1);
        QVERIFY(qRed(br) > 240);   // bottom-right = white from row 1[1]
    }
};
QTEST_MAIN(TestVideoSoftware)
#include "test_video_software.moc"
