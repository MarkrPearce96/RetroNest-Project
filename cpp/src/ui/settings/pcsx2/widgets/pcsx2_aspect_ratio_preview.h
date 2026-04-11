#pragma once
#include <QWidget>
#include <QRectF>

// Live aspect-ratio / crop / stretch / integer-scaling preview used
// inside Pcsx2GraphicsDisplayPage. Reproduces the draw-rect math from
// PCSX2 upstream GSRenderer::CalculateDrawDstRect +
// CalculateDrawSrcRect on a fixed 640x448 NTSC source, scaled down
// into a 16:9 widget. All state is held in this widget — callers use
// the setters below and the widget repaints itself.
class Pcsx2AspectRatioPreview : public QWidget {
    Q_OBJECT
public:
    // Matches the five options of the Display/AspectRatio combo.
    // Upstream string values: "Stretch", "Auto 4:3/3:2", "4:3", "16:9",
    // "10:7".
    enum class AspectRatio {
        Stretch,
        Auto4_3_3_2,
        R4_3,
        R16_9,
        R10_7
    };

    explicit Pcsx2AspectRatioPreview(QWidget* parent = nullptr);

    void setAspectRatio(AspectRatio ratio);
    void setStretchY(int percent);                 // 10..300, default 100
    void setCrop(int left, int top, int right, int bottom);
    void setIntegerScaling(bool on);

    // Convenience: map the schema's AspectRatio combo value string to
    // the enum. Unknown strings fall back to R4_3.
    static AspectRatio fromSchemaValue(const QString& v);

    QSize sizeHint() const override        { return QSize(320, 180); }
    QSize minimumSizeHint() const override { return QSize(240, 135); }

protected:
    void paintEvent(QPaintEvent* e) override;
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

private:
    // Pure-function draw-rect computation. Returns a QRectF in widget
    // coordinates representing where the game scene is painted. See the
    // comment block at the top of pcsx2_aspect_ratio_preview.cpp for
    // the step-by-step mapping to upstream GSRenderer.cpp.
    QRectF computeDrawRect(const QRectF& client) const;

    QString labelForCurrentRatio() const;

    AspectRatio m_ratio = AspectRatio::R4_3;
    int m_stretchY      = 100;
    int m_cropL = 0, m_cropT = 0, m_cropR = 0, m_cropB = 0;
    bool m_integerScaling = false;
};
