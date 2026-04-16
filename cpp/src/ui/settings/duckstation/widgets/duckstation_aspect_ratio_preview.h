#pragma once
#include <QWidget>
#include <QRectF>

// Live aspect-ratio preview used inside DuckStationGraphicsPage.
// Mirrors the structure of Pcsx2AspectRatioPreview but uses DuckStation's
// aspect-ratio values and image assets.
class DuckStationAspectRatioPreview : public QWidget {
    Q_OBJECT
public:
    // Matches DuckStation's DisplayAspectRatio enum.
    // Upstream string values: "Auto (Game Native)", "Stretch To Fill",
    // "4:3", "16:9", "19:9", "20:9", "21:9", "16:10", "PAR 1:1".
    enum class AspectRatio {
        Auto,
        Stretch,
        R4x3,
        R16x9,
        R19x9,
        R20x9,
        R21x9,
        R16x10,
        Par1x1,
    };

    explicit DuckStationAspectRatioPreview(QWidget* parent = nullptr);

    void setAspectRatio(AspectRatio ratio);
    void setCrop(int left, int top, int right, int bottom);

    // Returns the floating-point ratio for the current AspectRatio value.
    float ratioFloat() const;

    // Convenience: map the schema's AspectRatio combo value string to
    // the enum. Unknown strings fall back to Auto.
    static AspectRatio fromSchemaValue(const QString& v);

    QSize sizeHint() const override        { return QSize(320, 180); }
    QSize minimumSizeHint() const override { return QSize(240, 135); }

protected:
    void paintEvent(QPaintEvent* e) override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

private:
    QRectF computeDrawRect(const QRectF& client) const;
    QString labelForCurrentRatio() const;

    AspectRatio m_ratio = AspectRatio::Auto;
    int m_cropL = 0, m_cropT = 0, m_cropR = 0, m_cropB = 0;
};
