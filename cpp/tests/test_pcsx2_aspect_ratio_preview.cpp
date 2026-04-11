#include <QtTest>
#include <QApplication>
#include "ui/settings/pcsx2/widgets/pcsx2_aspect_ratio_preview.h"

class TestPcsx2AspectRatioPreview : public QObject {
    Q_OBJECT
private slots:
    void testSizeHint() {
        Pcsx2AspectRatioPreview w;
        QCOMPARE(w.sizeHint(),        QSize(320, 180));
        QCOMPARE(w.minimumSizeHint(), QSize(240, 135));
    }

    void testSettersDoNotCrash() {
        Pcsx2AspectRatioPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::Stretch);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::Auto4_3_3_2);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R4_3);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R16_9);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R10_7);

        w.setStretchY(10);
        w.setStretchY(130);
        w.setStretchY(300);
        w.setStretchY(-99);   // clamped to 10
        w.setStretchY(9999);  // clamped to 300

        w.setCrop(0, 0, 0, 0);
        w.setCrop(20, 30, 40, 50);
        w.setCrop(999, 999, 999, 999); // clamped to 100 per axis

        w.setIntegerScaling(true);
        w.setIntegerScaling(false);

        // Force a repaint to exercise computeDrawRect + paintEvent.
        w.repaint();
    }

    void testFromSchemaValue() {
        using AR = Pcsx2AspectRatioPreview::AspectRatio;
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("Stretch"),      AR::Stretch);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("Auto 4:3/3:2"), AR::Auto4_3_3_2);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("4:3"),          AR::R4_3);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("16:9"),         AR::R16_9);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("10:7"),         AR::R10_7);
        QCOMPARE(Pcsx2AspectRatioPreview::fromSchemaValue("garbage"),      AR::R4_3);
    }

    void testTinyClientDoesNotDivideByZero() {
        Pcsx2AspectRatioPreview w;
        w.resize(1, 1);
        w.setAspectRatio(Pcsx2AspectRatioPreview::AspectRatio::R16_9);
        w.setStretchY(300);
        w.setCrop(50, 50, 50, 50);
        w.setIntegerScaling(true);
        w.repaint();   // must not crash
    }
};

QTEST_MAIN(TestPcsx2AspectRatioPreview)
#include "test_pcsx2_aspect_ratio_preview.moc"
