#pragma once
#include <QWidget>
#include <QRectF>
#include <QStringList>

// Shared live OSD preview. Paints a dummy 16:9 frame and overlays a
// miniature of PCSX2 upstream's ImGuiOverlays layout: perf column
// (pos-driven), top-right indicators (hard corner), bottom-right
// settings (hard corner), bottom-left inputs (hard corner).
//
// Reference upstream file: pcsx2/ImGui/ImGuiOverlays.cpp
//   DrawPerformanceOverlay   (lines 191-557)
//   DrawIndicatorsOverlay    (lines 1201-1253)
//   DrawSettingsOverlay      (lines 780-929)
//   DrawInputsOverlay        (lines 931-...)
//   RenderOverlays dispatch  (lines 1682-1699)
class OsdPreview : public QWidget {
    Q_OBJECT
    // Q_PROPERTYs let GenericSettingsPage live-bind via setProperty() without
    // depending on this concrete class. Names match PreviewSpec.keyToProperty
    // values declared by adapters (see Pcsx2Adapter::previewSpec()).
    Q_PROPERTY(QString performancePos READ performancePosString WRITE setPerformancePosString)
    Q_PROPERTY(QString messagesPos READ messagesPosString WRITE setMessagesPosString)
    Q_PROPERTY(int osdScale READ osdScale WRITE setOsdScale)

    // Show* toggles — name matches PreviewSpec.keyToProperty values
    // declared by adapters (see Pcsx2Adapter::previewSpec()).
    Q_PROPERTY(bool showFps READ showFps WRITE setShowFps)
    Q_PROPERTY(bool showVps READ showVps WRITE setShowVps)
    Q_PROPERTY(bool showSpeed READ showSpeed WRITE setShowSpeed)
    Q_PROPERTY(bool showVersion READ showVersion WRITE setShowVersion)
    Q_PROPERTY(bool showResolution READ showResolution WRITE setShowResolution)
    Q_PROPERTY(bool showHardwareInfo READ showHardwareInfo WRITE setShowHardwareInfo)
    Q_PROPERTY(bool showCpu READ showCpu WRITE setShowCpu)
    Q_PROPERTY(bool showGpu READ showGpu WRITE setShowGpu)
    Q_PROPERTY(bool showFrameTimes READ showFrameTimes WRITE setShowFrameTimes)
    Q_PROPERTY(bool showGsStats READ showGsStats WRITE setShowGsStats)
    Q_PROPERTY(bool showIndicators READ showIndicators WRITE setShowIndicators)
    Q_PROPERTY(bool showVideoCapture READ showVideoCapture WRITE setShowVideoCapture)
    Q_PROPERTY(bool showInputRec READ showInputRec WRITE setShowInputRec)
    Q_PROPERTY(bool showTextureReplacements READ showTextureReplacements WRITE setShowTextureReplacements)
    Q_PROPERTY(bool showSettings READ showSettings WRITE setShowSettings)
    Q_PROPERTY(bool showPatches READ showPatches WRITE setShowPatches)
    Q_PROPERTY(bool showInputs READ showInputs WRITE setShowInputs)
public:
    enum class OverlayPos {
        None,
        TopLeft, TopCenter, TopRight,
        CenterLeft, Center, CenterRight,
        BottomLeft, BottomCenter, BottomRight
    };

    explicit OsdPreview(QWidget* parent = nullptr);

    void setPerformancePos(OverlayPos pos);
    void setMessagesPos(OverlayPos pos);

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

    // Q_PROPERTY string adapters — accept the schema's string value and
    // route to the typed setter.
    void setPerformancePosString(const QString& v);
    QString performancePosString() const;
    void setMessagesPosString(const QString& v);
    QString messagesPosString() const;

    // Q_PROPERTY read accessors
    bool showFps() const              { return m_s.fps; }
    bool showVps() const              { return m_s.vps; }
    bool showSpeed() const            { return m_s.speed; }
    bool showVersion() const          { return m_s.version; }
    bool showResolution() const       { return m_s.resolution; }
    bool showHardwareInfo() const     { return m_s.hardwareInfo; }
    bool showCpu() const              { return m_s.cpu; }
    bool showGpu() const              { return m_s.gpu; }
    bool showFrameTimes() const       { return m_s.frameTimes; }
    bool showGsStats() const          { return m_s.gsStats; }
    bool showIndicators() const       { return m_s.indicators; }
    bool showVideoCapture() const     { return m_s.videoCapture; }
    bool showInputRec() const         { return m_s.inputRec; }
    bool showTextureReplacements() const { return m_s.textureReplacements; }
    bool showSettings() const         { return m_s.settings; }
    bool showPatches() const          { return m_s.patches; }
    bool showInputs() const           { return m_s.inputs; }
    int osdScale() const              { return m_s.osdScale; }

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
        OverlayPos messagesPos = OverlayPos::TopLeft;  // held for API/Q_PROPERTY; not consumed by paintEvent yet
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
