#include <QtTest>
#include <QApplication>
#include "ui/settings/widgets/preview/aspect_ratio_preview.h"

class TestAspectRatioPreview : public QObject {
    Q_OBJECT
private slots:
    void testSizeHint() {
        AspectRatioPreview w;
        QCOMPARE(w.sizeHint(),        QSize(320, 180));
        QCOMPARE(w.minimumSizeHint(), QSize(240, 135));
    }

    void testSettersDoNotCrash() {
        AspectRatioPreview w;
        w.resize(320, 180);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        w.setAspectRatio(AspectRatioPreview::AspectRatio::Stretch);
        w.setAspectRatio(AspectRatioPreview::AspectRatio::Auto4_3_3_2);
        w.setAspectRatio(AspectRatioPreview::AspectRatio::R4_3);
        w.setAspectRatio(AspectRatioPreview::AspectRatio::R16_9);
        w.setAspectRatio(AspectRatioPreview::AspectRatio::R10_7);

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
        using AR = AspectRatioPreview::AspectRatio;
        QCOMPARE(AspectRatioPreview::fromSchemaValue("Stretch"),      AR::Stretch);
        QCOMPARE(AspectRatioPreview::fromSchemaValue("Auto 4:3/3:2"), AR::Auto4_3_3_2);
        QCOMPARE(AspectRatioPreview::fromSchemaValue("4:3"),          AR::R4_3);
        QCOMPARE(AspectRatioPreview::fromSchemaValue("16:9"),         AR::R16_9);
        QCOMPARE(AspectRatioPreview::fromSchemaValue("10:7"),         AR::R10_7);
        QCOMPARE(AspectRatioPreview::fromSchemaValue("garbage"),      AR::R4_3);
    }

    void testTinyClientDoesNotDivideByZero() {
        AspectRatioPreview w;
        w.resize(1, 1);
        w.setAspectRatio(AspectRatioPreview::AspectRatio::R16_9);
        w.setStretchY(300);
        w.setCrop(50, 50, 50, 50);
        w.setIntegerScaling(true);
        w.repaint();   // must not crash
    }

    void testQPropertyRoundTrip() {
        AspectRatioPreview w;
        // String aspect mode property
        QVERIFY(w.setProperty("aspectMode", "16:9"));
        QCOMPARE(w.property("aspectMode").toString(), QString("16:9"));
        // Int crop property — single edge
        QVERIFY(w.setProperty("cropL", 25));
        QCOMPARE(w.property("cropL").toInt(), 25);
        // Bool integer scaling
        QVERIFY(w.setProperty("integerScaling", true));
        QCOMPARE(w.property("integerScaling").toBool(), true);
        // stretchY clamp: 999 → 300
        QVERIFY(w.setProperty("stretchY", 999));
        QCOMPARE(w.property("stretchY").toInt(), 300);
        // FMV aspect mode round-trip — same enum mapping as aspectMode.
        QVERIFY(w.setProperty("fmvAspectMode", "Auto 4:3/3:2"));
        QCOMPARE(w.property("fmvAspectMode").toString(), QString("Auto 4:3/3:2"));
    }
};

QTEST_MAIN(TestAspectRatioPreview)
#include "test_aspect_ratio_preview.moc"
