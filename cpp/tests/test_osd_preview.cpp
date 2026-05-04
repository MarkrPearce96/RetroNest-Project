#include <QtTest>
#include <QApplication>
#include "ui/settings/widgets/preview/osd_preview.h"

class TestOsdPreview : public QObject {
    Q_OBJECT
private slots:
    void testSizeHint() {
        OsdPreview w;
        QCOMPARE(w.sizeHint(),        QSize(320, 180));
        QCOMPARE(w.minimumSizeHint(), QSize(240, 135));
        QVERIFY(w.hasHeightForWidth());
        QCOMPARE(w.heightForWidth(320), 180);
    }

    void testSettersDoNotCrash() {
        OsdPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        using Pos = OsdPreview::OverlayPos;
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
        using Pos = OsdPreview::OverlayPos;
        // Text labels
        QCOMPARE(OsdPreview::fromPosValue("None"),                  Pos::None);
        QCOMPARE(OsdPreview::fromPosValue("Top Left"),              Pos::TopLeft);
        QCOMPARE(OsdPreview::fromPosValue("Top Left (Default)"),    Pos::TopLeft);
        QCOMPARE(OsdPreview::fromPosValue("Top Center"),            Pos::TopCenter);
        QCOMPARE(OsdPreview::fromPosValue("Top Right"),             Pos::TopRight);
        QCOMPARE(OsdPreview::fromPosValue("Top Right (Default)"),   Pos::TopRight);
        QCOMPARE(OsdPreview::fromPosValue("Center Left"),           Pos::CenterLeft);
        QCOMPARE(OsdPreview::fromPosValue("Center"),                Pos::Center);
        QCOMPARE(OsdPreview::fromPosValue("Center Right"),          Pos::CenterRight);
        QCOMPARE(OsdPreview::fromPosValue("Bottom Left"),           Pos::BottomLeft);
        QCOMPARE(OsdPreview::fromPosValue("Bottom Center"),         Pos::BottomCenter);
        QCOMPARE(OsdPreview::fromPosValue("Bottom Right"),          Pos::BottomRight);
        QCOMPARE(OsdPreview::fromPosValue("garbage"),               Pos::TopLeft);

        // Numeric INI values (what the combo row actually emits)
        QCOMPARE(OsdPreview::fromPosValue("0"), Pos::None);
        QCOMPARE(OsdPreview::fromPosValue("1"), Pos::TopLeft);
        QCOMPARE(OsdPreview::fromPosValue("2"), Pos::TopCenter);
        QCOMPARE(OsdPreview::fromPosValue("3"), Pos::TopRight);
        QCOMPARE(OsdPreview::fromPosValue("4"), Pos::CenterLeft);
        QCOMPARE(OsdPreview::fromPosValue("5"), Pos::Center);
        QCOMPARE(OsdPreview::fromPosValue("6"), Pos::CenterRight);
        QCOMPARE(OsdPreview::fromPosValue("7"), Pos::BottomLeft);
        QCOMPARE(OsdPreview::fromPosValue("8"), Pos::BottomCenter);
        QCOMPARE(OsdPreview::fromPosValue("9"), Pos::BottomRight);
    }

    void testAllTogglesOnAtOnce() {
        OsdPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setPerformancePos(OsdPreview::OverlayPos::Center);
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

    void testQPropertyRoundTrip() {
        OsdPreview w;
        // Bool show toggle
        QVERIFY(w.setProperty("showFps", true));
        QCOMPARE(w.property("showFps").toBool(), true);
        QVERIFY(w.setProperty("showCpu", false));
        QCOMPARE(w.property("showCpu").toBool(), false);
        // Int osdScale
        QVERIFY(w.setProperty("osdScale", 150));
        QCOMPARE(w.property("osdScale").toInt(), 150);
        // String performancePos round-trip — exercise both setter and getter
        // switches. Use a value the existing fromPosValue accepts.
        QVERIFY(w.setProperty("performancePos", "Top Right"));
        QCOMPARE(w.property("performancePos").toString(), QString("Top Right"));
        // messagesPos uses the same conversion path — guard against any
        // setter/getter divergence.
        QVERIFY(w.setProperty("messagesPos", "Bottom Left"));
        QCOMPARE(w.property("messagesPos").toString(), QString("Bottom Left"));
    }
};

QTEST_MAIN(TestOsdPreview)
#include "test_osd_preview.moc"
