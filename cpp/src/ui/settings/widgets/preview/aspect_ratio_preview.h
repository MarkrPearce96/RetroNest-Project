#pragma once
#include <QWidget>
#include <QRectF>

// Live aspect-ratio / crop / stretch / integer-scaling preview.
// Reproduces the draw-rect math from PCSX2 upstream GSRenderer on a fixed
// 640x448 NTSC source, scaled into a 16:9 widget. Every input is optional
// — unset values mean "feature absent" and the corresponding overlay is
// not drawn. PCSX2 sets every input; emulators with a narrower feature
// surface (e.g. Dolphin) set only what they expose.
class AspectRatioPreview : public QWidget {
    Q_OBJECT
    // Q_PROPERTYs let GenericSettingsPage live-bind via setProperty() without
    // depending on this concrete class. Names match PreviewSpec.keyToProperty
    // values declared by adapters.
    Q_PROPERTY(QString aspectMode READ aspectModeString WRITE setAspectModeString)
    Q_PROPERTY(int stretchY READ stretchY WRITE setStretchY)
    Q_PROPERTY(int cropL READ cropL WRITE setCropL)
    Q_PROPERTY(int cropT READ cropT WRITE setCropT)
    Q_PROPERTY(int cropR READ cropR WRITE setCropR)
    Q_PROPERTY(int cropB READ cropB WRITE setCropB)
    Q_PROPERTY(bool integerScaling READ integerScaling WRITE setIntegerScaling)
    Q_PROPERTY(QString fmvAspectMode READ fmvAspectModeString WRITE setFmvAspectModeString)
public:
    enum class AspectRatio {
        Stretch,
        Auto4_3_3_2,
        R4_3,
        R16_9,
        R10_7
    };

    explicit AspectRatioPreview(QWidget* parent = nullptr);

    // Direct typed setters — preferred when the caller already has the enum.
    void setAspectRatio(AspectRatio ratio);
    void setStretchY(int percent);                           // 10..300, default 100
    void setCrop(int left, int top, int right, int bottom);
    void setIntegerScaling(bool on);

    // Per-edge crop setters — used by Q_PROPERTY binding so each crop
    // edge can map to its own SettingDef key.
    void setCropL(int v) { setCrop(v, m_cropT, m_cropR, m_cropB); }
    void setCropT(int v) { setCrop(m_cropL, v, m_cropR, m_cropB); }
    void setCropR(int v) { setCrop(m_cropL, m_cropT, v, m_cropB); }
    void setCropB(int v) { setCrop(m_cropL, m_cropT, m_cropR, v); }

    // Q_PROPERTY string adapters — accept the schema's string value
    // (e.g. "16:9") and route to the typed setter.
    void setAspectModeString(const QString& v);
    QString aspectModeString() const;
    void setFmvAspectModeString(const QString& v);
    QString fmvAspectModeString() const;

    int stretchY() const         { return m_stretchY; }
    int cropL() const            { return m_cropL; }
    int cropT() const            { return m_cropT; }
    int cropR() const            { return m_cropR; }
    int cropB() const            { return m_cropB; }
    bool integerScaling() const  { return m_integerScaling; }

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

    AspectRatio m_ratio = AspectRatio::R4_3;
    AspectRatio m_fmvRatio = AspectRatio::R4_3;  // unused by paintEvent today; kept for API
    int m_stretchY      = 100;
    int m_cropL = 0, m_cropT = 0, m_cropR = 0, m_cropB = 0;
    bool m_integerScaling = false;
};
