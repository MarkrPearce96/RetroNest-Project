#include <QtTest>
#include <QApplication>
#include "ui/settings/pcsx2/widgets/pcsx2_osd_preview.h"

class TestPcsx2OsdPreview : public QObject {
    Q_OBJECT
private slots:
    void testSizeHint() {
        Pcsx2OsdPreview w;
        QCOMPARE(w.sizeHint(),        QSize(320, 180));
        QCOMPARE(w.minimumSizeHint(), QSize(240, 135));
        QVERIFY(w.hasHeightForWidth());
        QCOMPARE(w.heightForWidth(320), 180);
    }

    void testSettersDoNotCrash() {
        Pcsx2OsdPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        using Pos = Pcsx2OsdPreview::OverlayPos;
        for (Pos p : {Pos::None, Pos::TopLeft, Pos::TopCenter, Pos::TopRight,
                      Pos::CenterLeft, Pos::Center, Pos::CenterRight,
                      Pos::BottomLeft, Pos::BottomCenter, Pos::BottomRight}) {
            w.setPerformancePos(p);
        }

        w.setOsdScale(-500);
        w.setOsdScale(0);
        w.setOsdScale(100);
        w.setOsdScale(300);
        w.setOsdScale(9999);

        w.setShowFps(true);        w.setShowFps(false);
        w.setShowVps(true);        w.setShowVps(false);
        w.setShowSpeed(true);      w.setShowSpeed(false);
        w.setShowVersion(true);    w.setShowVersion(false);
        w.setShowResolution(true); w.setShowResolution(false);
        w.setShowHardwareInfo(true);
        w.setShowCpu(true);        w.setShowGpu(true);
        w.setShowFrameTimes(true); w.setShowGsStats(true);

        w.setShowIndicators(true);
        w.setShowVideoCapture(true);
        w.setShowInputRec(true);
        w.setShowTextureReplacements(true);
        w.setShowSettings(true);
        w.setShowPatches(true);
        w.setShowInputs(true);

        w.repaint();
    }

    void testFromPosValue() {
        using Pos = Pcsx2OsdPreview::OverlayPos;
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("None"),                  Pos::None);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Left"),              Pos::TopLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Left (Default)"),    Pos::TopLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Center"),            Pos::TopCenter);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Right"),             Pos::TopRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Top Right (Default)"),   Pos::TopRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Center Left"),           Pos::CenterLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Center"),                Pos::Center);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Center Right"),          Pos::CenterRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Bottom Left"),           Pos::BottomLeft);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Bottom Center"),         Pos::BottomCenter);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("Bottom Right"),          Pos::BottomRight);
        QCOMPARE(Pcsx2OsdPreview::fromPosValue("garbage"),               Pos::TopLeft);
    }

    void testAllTogglesOnAtOnce() {
        Pcsx2OsdPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setPerformancePos(Pcsx2OsdPreview::OverlayPos::Center);
        w.setOsdScale(150);
        w.setShowFps(true);
        w.setShowVps(true);
        w.setShowSpeed(true);
        w.setShowVersion(true);
        w.setShowResolution(true);
        w.setShowHardwareInfo(true);
        w.setShowCpu(true);
        w.setShowGpu(true);
        w.setShowFrameTimes(true);
        w.setShowGsStats(true);
        w.setShowIndicators(true);
        w.setShowVideoCapture(true);
        w.setShowInputRec(true);
        w.setShowTextureReplacements(true);
        w.setShowSettings(true);
        w.setShowPatches(true);
        w.setShowInputs(true);

        w.repaint();
    }
};

QTEST_MAIN(TestPcsx2OsdPreview)
#include "test_pcsx2_osd_preview.moc"
