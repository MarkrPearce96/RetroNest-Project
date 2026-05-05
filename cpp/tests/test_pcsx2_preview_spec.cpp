#include <QtTest>
#include "adapters/pcsx2_adapter.h"

class TestPcsx2PreviewSpec : public QObject {
    Q_OBJECT
private slots:
    void testRecommendedReturnsAspectPreview() {
        // Aspect preview lives on Recommended now — Graphics > Display has
        // the full crop/stretch/integer-scaling controls but no preview,
        // matching Dolphin's split.
        PCSX2Adapter a;
        const auto spec = a.previewSpec("Recommended", "");
        QCOMPARE(spec.previewType, QString("aspect"));
        QCOMPARE(spec.keyToProperty.value("AspectRatio"), QString("aspectMode"));
    }

    void testDisplayHasNoPreview() {
        PCSX2Adapter a;
        QVERIFY(a.previewSpec("Graphics", "Display").previewType.isEmpty());
    }

    void testOsdReturnsOsdPreview() {
        // OSD lives under Graphics > On-Screen Display (Dolphin-style sub-tab).
        PCSX2Adapter a;
        const auto spec = a.previewSpec("Graphics", "On-Screen Display");
        QCOMPARE(spec.previewType, QString("osd"));
        QCOMPARE(spec.keyToProperty.value("OsdShowFPS"),    QString("showFps"));
        QCOMPARE(spec.keyToProperty.value("OsdShowCPU"),    QString("showCpu"));
        QCOMPARE(spec.keyToProperty.value("OsdMessagesPos"),    QString("messagesPos"));
        QCOMPARE(spec.keyToProperty.value("OsdPerformancePos"), QString("performancePos"));
    }

    void testUnknownCategoryReturnsEmpty() {
        PCSX2Adapter a;
        QVERIFY(a.previewSpec("Audio", "").previewType.isEmpty());
        QVERIFY(a.previewSpec("Graphics", "Rendering").previewType.isEmpty());
        // Old top-level "On-Screen Display" routing should no longer match.
        QVERIFY(a.previewSpec("On-Screen Display", "").previewType.isEmpty());
    }
};

QTEST_MAIN(TestPcsx2PreviewSpec)
#include "test_pcsx2_preview_spec.moc"
