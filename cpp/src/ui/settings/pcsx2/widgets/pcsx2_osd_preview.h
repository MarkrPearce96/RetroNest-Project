#pragma once
#include <QWidget>
#include <QRectF>
#include <QStringList>

// Live OSD preview used inside Pcsx2GraphicsOsdPage. Paints a dummy 16:9
// PS2 frame and overlays a miniature of PCSX2 upstream's ImGuiOverlays
// layout: perf column (pos-driven), top-right indicators (hard corner),
// bottom-right settings (hard corner), bottom-left inputs (hard corner).
//
// Reference upstream file: pcsx2/ImGui/ImGuiOverlays.cpp
//   DrawPerformanceOverlay   (lines 191-557)
//   DrawIndicatorsOverlay    (lines 1201-1253)
//   DrawSettingsOverlay      (lines 780-929)
//   DrawInputsOverlay        (lines 931-...)
//   RenderOverlays dispatch  (lines 1682-1699)
class Pcsx2OsdPreview : public QWidget {
    Q_OBJECT
public:
    enum class OverlayPos {
        None,
        TopLeft, TopCenter, TopRight,
        CenterLeft, Center, CenterRight,
        BottomLeft, BottomCenter, BottomRight
    };

    explicit Pcsx2OsdPreview(QWidget* parent = nullptr);

    void setPerformancePos(OverlayPos pos);

    void setShowFps(bool on);
    void setShowVps(bool on);
    void setShowSpeed(bool on);
    void setShowVersion(bool on);
    void setShowResolution(bool on);
    void setShowHardwareInfo(bool on);
    void setShowCpu(bool on);
    void setShowGpu(bool on);
    void setShowFrameTimes(bool on);
    void setShowGsStats(bool on);

    void setShowIndicators(bool on);
    void setShowVideoCapture(bool on);
    void setShowInputRec(bool on);
    void setShowTextureReplacements(bool on);
    void setShowSettings(bool on);
    void setShowPatches(bool on);
    void setShowInputs(bool on);

    void setOsdScale(int percent);

    static OverlayPos fromPosValue(const QString& v);

    QSize sizeHint() const override        { return QSize(320, 180); }
    QSize minimumSizeHint() const override { return QSize(240, 135); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w * 9 / 16; }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    struct State {
        OverlayPos perfPos = OverlayPos::TopRight;
        bool fps = false, vps = false, speed = false, version = false;
        bool resolution = false, hardwareInfo = false;
        bool cpu = false, gpu = false, frameTimes = false, gsStats = false;
        bool indicators = true, videoCapture = false, inputRec = false,
             textureReplacements = false;
        bool settings = false, patches = false, inputs = false;
        int osdScale = 100;
    } m_s;

    int  scaledFontPx() const;
    void paintGameScene(QPainter& p, const QRectF& r) const;

    QStringList buildPerfColumnLines() const;
    void drawPerfColumn(QPainter& p, const QRectF& screen) const;
    void drawTopRightIndicators(QPainter& p, const QRectF& screen) const;
    void drawBottomRightSettings(QPainter& p, const QRectF& screen) const;
    void drawBottomLeftInputs(QPainter& p, const QRectF& screen) const;
};
