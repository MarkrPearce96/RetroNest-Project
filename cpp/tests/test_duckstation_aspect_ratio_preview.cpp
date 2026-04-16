#include <QTest>
#include "ui/settings/duckstation/widgets/duckstation_aspect_ratio_preview.h"

class TestDuckStationAspectRatioPreview : public QObject {
    Q_OBJECT
private slots:
    void fromSchemaValue_mapsKnownStrings() {
        using AR = DuckStationAspectRatioPreview::AspectRatio;
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("4:3"),             AR::R4x3);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("16:9"),            AR::R16x9);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("21:9"),            AR::R21x9);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("Stretch To Fill"), AR::Stretch);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("Auto (Game Native)"), AR::Auto);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("PAR 1:1"),         AR::Par1x1);
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("16:10"),           AR::R16x10);
    }
    void fromSchemaValue_unknownFallsBackToAuto() {
        using AR = DuckStationAspectRatioPreview::AspectRatio;
        QCOMPARE(DuckStationAspectRatioPreview::fromSchemaValue("nonsense"), AR::Auto);
    }
    void ratioFloat_returnsCorrectValues() {
        DuckStationAspectRatioPreview w;
        w.setAspectRatio(DuckStationAspectRatioPreview::AspectRatio::R16x9);
        QCOMPARE(w.ratioFloat(), 16.0f / 9.0f);
        w.setAspectRatio(DuckStationAspectRatioPreview::AspectRatio::R4x3);
        QCOMPARE(w.ratioFloat(), 4.0f / 3.0f);
    }
};
QTEST_MAIN(TestDuckStationAspectRatioPreview)
#include "test_duckstation_aspect_ratio_preview.moc"
