#pragma once
#include <QWidget>
#include <QRectF>
#include <QString>

// Live OSD preview used inside DuckStationGraphicsOsdPage. Paints a dummy 16:9
// PS1 frame and overlays a miniature of DuckStation's actual OSD layout, as
// implemented in upstream src/core/imgui_overlays.cpp:
//
//   top-right   — performance stats (FPS/VPS, Speed, CPU, GPU, Resolution,
//                 GPU Stats, Frame Times, Latency Stats) and status
//                 indicators (FF / REC), stacked vertically downward
//   bottom-left — controller inputs
//   bottom-right— enhancements summary
//   configurable — OSD messages (TopLeft / TopCenter / TopRight /
//                  BottomLeft / BottomCenter / BottomRight) via
//                  OSDMessageLocation
//
// Toggle bools mirror the "On-Screen Display" / "Overlays" schema keys.
class DuckStationOsdPreview : public QWidget {
    Q_OBJECT
public:
    explicit DuckStationOsdPreview(QWidget* parent = nullptr);

    // Overlay toggles — each maps to a schema bool key
    void setShowFPS(bool on);
    void setShowSpeed(bool on);
    void setShowCPU(bool on);
    void setShowGPU(bool on);
    void setShowResolution(bool on);
    void setShowGPUStatistics(bool on);
    void setShowFrameTimes(bool on);
    void setShowLatencyStatistics(bool on);
    void setShowInputs(bool on);
    void setShowEnhancements(bool on);
    void setShowOSDMessages(bool on);
    void setShowStatusIndicators(bool on);

    // OSD message anchor — accepts the OSDMessageLocation INI value
    // (TopLeft / TopCenter / TopRight / BottomLeft / BottomCenter / BottomRight).
    void setMessageLocation(const QString& loc);

    // Scale / margin (OSDScale / OSDMargin schema keys)
    void setScale(int percent);
    void setMargin(int pixels);

    QSize sizeHint()        const override { return QSize(320, 180); }
    QSize minimumSizeHint() const override { return QSize(240, 135); }
    bool  hasHeightForWidth() const override { return true; }
    int   heightForWidth(int w) const override { return w * 9 / 16; }

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    struct State {
        bool fps = false, speed = false;
        bool cpu = false, gpu = false;
        bool resolution = false, gpuStats = false;
        bool frameTimes = false, latencyStats = false;
        bool inputs = false, enhancements = false;
        bool osdMessages = true, statusIndicators = true;
        int  scale  = 100;
        int  margin = 10;
        QString messageLocation = QStringLiteral("TopLeft");
    } m_s;

    int  scaledFontPx() const;
    void paintGameScene(QPainter& p, const QRectF& r) const;
    void drawTopRightStack(QPainter& p, const QRectF& screen) const;
    void drawBottomLeftInputs(QPainter& p, const QRectF& screen) const;
    void drawBottomRightEnhancements(QPainter& p, const QRectF& screen) const;
    void drawOsdMessage(QPainter& p, const QRectF& screen) const;
};
