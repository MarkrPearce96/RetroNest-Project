#include "ui/settings/widgets/preview/osd_preview.h"
#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QFontMetricsF>
#include <QFontDatabase>
#include <QFontInfo>
#include <QStringList>
#include <QtMath>
#include <algorithm>

namespace {
constexpr qreal kMargin = 8.0;

QColor kShadow(0, 0, 0, 217);
QColor kWhite(0xff, 0xff, 0xff);
QColor kSpeedGreen(0x60, 0xff, 0x60);
QColor kDimRed  (0xff, 0x60, 0x60);
QColor kMuted   (0xd9, 0xd4, 0xcc);

QFont monospaceFont(int px) {
    for (const char* name : {"Menlo", "Monaco", "Courier New"}) {
        QFont f(QString::fromLatin1(name));
        if (QFontInfo(f).fixedPitch()) {
            f.setPixelSize(px);
            return f;
        }
    }
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPixelSize(px);
    return f;
}

void drawShadowedText(QPainter& p, const QPointF& baseline,
                      const QString& text, const QColor& color) {
    p.setPen(kShadow);
    p.drawText(baseline + QPointF(1, 1), text);
    p.setPen(color);
    p.drawText(baseline, text);
}

QString posToString(OsdPreview::OverlayPos pos) {
    using P = OsdPreview::OverlayPos;
    switch (pos) {
        case P::None:         return "None";
        case P::TopLeft:      return "Top Left";
        case P::TopCenter:    return "Top Center";
        case P::TopRight:     return "Top Right";
        case P::CenterLeft:   return "Center Left";
        case P::Center:       return "Center";
        case P::CenterRight:  return "Center Right";
        case P::BottomLeft:   return "Bottom Left";
        case P::BottomCenter: return "Bottom Center";
        case P::BottomRight:  return "Bottom Right";
    }
    return "None";
}
} // namespace

OsdPreview::OsdPreview(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void OsdPreview::setPerformancePos(OverlayPos pos) { m_s.perfPos = pos; update(); }

void OsdPreview::setMessagesPos(OverlayPos pos) {
    m_s.messagesPos = pos;
    // Intentionally no update() — m_s.messagesPos is held for API/Q_PROPERTY
    // round-trip but is not yet consumed by paintEvent. Add update() here
    // when paintEvent starts using m_s.messagesPos.
}

void OsdPreview::setShowFps(bool on)             { m_s.fps = on;             update(); }
void OsdPreview::setShowVps(bool on)             { m_s.vps = on;             update(); }
void OsdPreview::setShowSpeed(bool on)           { m_s.speed = on;           update(); }
void OsdPreview::setShowVersion(bool on)         { m_s.version = on;         update(); }
void OsdPreview::setShowResolution(bool on)      { m_s.resolution = on;      update(); }
void OsdPreview::setShowHardwareInfo(bool on)    { m_s.hardwareInfo = on;    update(); }
void OsdPreview::setShowCpu(bool on)             { m_s.cpu = on;             update(); }
void OsdPreview::setShowGpu(bool on)             { m_s.gpu = on;             update(); }
void OsdPreview::setShowFrameTimes(bool on)      { m_s.frameTimes = on;      update(); }
void OsdPreview::setShowGsStats(bool on)         { m_s.gsStats = on;         update(); }

void OsdPreview::setShowIndicators(bool on)         { m_s.indicators = on;           update(); }
void OsdPreview::setShowVideoCapture(bool on)       { m_s.videoCapture = on;         update(); }
void OsdPreview::setShowInputRec(bool on)           { m_s.inputRec = on;             update(); }
void OsdPreview::setShowTextureReplacements(bool on){ m_s.textureReplacements = on;  update(); }
void OsdPreview::setShowSettings(bool on)           { m_s.settings = on;             update(); }
void OsdPreview::setShowPatches(bool on)            { m_s.patches = on;              update(); }
void OsdPreview::setShowInputs(bool on)             { m_s.inputs = on;               update(); }

void OsdPreview::setOsdScale(int percent) {
    m_s.osdScale = std::clamp(percent, 10, 300);
    update();
}

OsdPreview::OverlayPos OsdPreview::fromPosValue(const QString& v) {
    const QString s = v.trimmed();

    // Handle numeric INI values (0-9) used by PCSX2's OsdPerformancePos / OsdMessagesPos
    bool ok = false;
    const int num = s.toInt(&ok);
    if (ok) {
        switch (num) {
            case 0: return OverlayPos::None;
            case 1: return OverlayPos::TopLeft;
            case 2: return OverlayPos::TopCenter;
            case 3: return OverlayPos::TopRight;
            case 4: return OverlayPos::CenterLeft;
            case 5: return OverlayPos::Center;
            case 6: return OverlayPos::CenterRight;
            case 7: return OverlayPos::BottomLeft;
            case 8: return OverlayPos::BottomCenter;
            case 9: return OverlayPos::BottomRight;
            default: return OverlayPos::TopLeft;
        }
    }

    // Handle text labels (e.g. from combo display text or "(Default)" suffixed labels)
    if (s.compare("None", Qt::CaseInsensitive) == 0) return OverlayPos::None;
    const QString stem = s.section('(', 0, 0).trimmed();
    if (stem.compare("Top Left",      Qt::CaseInsensitive) == 0) return OverlayPos::TopLeft;
    if (stem.compare("Top Center",    Qt::CaseInsensitive) == 0) return OverlayPos::TopCenter;
    if (stem.compare("Top Right",     Qt::CaseInsensitive) == 0) return OverlayPos::TopRight;
    if (stem.compare("Center Left",   Qt::CaseInsensitive) == 0) return OverlayPos::CenterLeft;
    if (stem.compare("Center",        Qt::CaseInsensitive) == 0) return OverlayPos::Center;
    if (stem.compare("Center Right",  Qt::CaseInsensitive) == 0) return OverlayPos::CenterRight;
    if (stem.compare("Bottom Left",   Qt::CaseInsensitive) == 0) return OverlayPos::BottomLeft;
    if (stem.compare("Bottom Center", Qt::CaseInsensitive) == 0) return OverlayPos::BottomCenter;
    if (stem.compare("Bottom Right",  Qt::CaseInsensitive) == 0) return OverlayPos::BottomRight;
    return OverlayPos::TopLeft;
}

void OsdPreview::setPerformancePosString(const QString& v) {
    setPerformancePos(fromPosValue(v));
}
QString OsdPreview::performancePosString() const { return posToString(m_s.perfPos); }

void OsdPreview::setMessagesPosString(const QString& v) {
    setMessagesPos(fromPosValue(v));
}
QString OsdPreview::messagesPosString() const { return posToString(m_s.messagesPos); }

int OsdPreview::scaledFontPx() const {
    const int px = int(qRound(10.0 * double(m_s.osdScale) / 100.0));
    return std::clamp(px, 6, 24);
}

void OsdPreview::paintGameScene(QPainter& p, const QRectF& r) const {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0,  QColor(0x5e, 0x7e, 0xa6));
    g.setColorAt(0.55, QColor(0x8a, 0x70, 0x58));
    g.setColorAt(1.0,  QColor(0x3a, 0x2c, 0x22));
    p.fillRect(r, g);

    const qreal sunR = qMin(r.width(), r.height()) * 0.08;
    QPointF sunC(r.left() + r.width() * 0.72, r.top() + r.height() * 0.32);
    QRadialGradient sg(sunC, sunR * 2.2);
    sg.setColorAt(0.0, QColor(255, 235, 180, 230));
    sg.setColorAt(1.0, QColor(255, 200, 120, 0));
    p.setBrush(sg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(sunC, sunR * 2.2, sunR * 2.2);
}

QStringList OsdPreview::buildPerfColumnLines() const {
    QStringList out;

    QStringList line1;
    if (m_s.speed)   line1 << QStringLiteral("Speed: 100%");
    if (m_s.vps)     line1 << QStringLiteral("VPS: 59.94");
    if (m_s.fps)     line1 << QStringLiteral("FPS: 59.94");
    if (m_s.version) line1 << QStringLiteral("PCSX2 2.3.0");
    if (!line1.isEmpty())
        out << line1.join(QStringLiteral(" | "));

    if (m_s.gsStats) {
        out << QStringLiteral("GS: 4328 draws")
            << QStringLiteral("VRAM: 384 MB / 512 MB")
            << QStringLiteral("6 QF | Min 14.2ms | Avg 21.4ms | Max 32.8ms");
    }
    if (m_s.resolution)
        out << QStringLiteral("640x448 NTSC Interlaced");
    if (m_s.hardwareInfo) {
        out << QStringLiteral("CPU: Apple M1 Max (10C/10T)")
            << QStringLiteral("GPU: Apple M1 Max");
    }
    if (m_s.cpu)
        out << QStringLiteral("EE: 32.5% (5.42ms)  GS: 14.2% (2.36ms)");
    if (m_s.gpu)
        out << QStringLiteral("GPU: 42.3% (4.21ms)");
    if (m_s.frameTimes)
        out << QStringLiteral("[▁▂▃▄▅▆▇█"
                              "▇▆▅▄▃▂▁]");
    return out;
}

void OsdPreview::drawPerfColumn(QPainter& p, const QRectF& screen) const {
    if (m_s.perfPos == OverlayPos::None) return;
    const QStringList lines = buildPerfColumnLines();
    if (lines.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);

    qreal maxW = 0.0;
    for (const QString& l : lines)
        maxW = std::max(maxW, fm.horizontalAdvance(l));
    const qreal lineH = fm.height();
    const qreal blockH = lineH * lines.size();
    const qreal blockW = maxW;

    qreal x = screen.left() + kMargin;
    qreal y = screen.top()  + kMargin + fm.ascent();

    switch (m_s.perfPos) {
        case OverlayPos::TopLeft:
        case OverlayPos::CenterLeft:
        case OverlayPos::BottomLeft:
            x = screen.left() + kMargin; break;
        case OverlayPos::TopCenter:
        case OverlayPos::Center:
        case OverlayPos::BottomCenter:
            x = screen.left() + (screen.width() - blockW) * 0.5; break;
        case OverlayPos::TopRight:
        case OverlayPos::CenterRight:
        case OverlayPos::BottomRight:
            x = screen.right() - kMargin - blockW; break;
        default: break;
    }
    switch (m_s.perfPos) {
        case OverlayPos::TopLeft:
        case OverlayPos::TopCenter:
        case OverlayPos::TopRight:
            y = screen.top() + kMargin + fm.ascent(); break;
        case OverlayPos::CenterLeft:
        case OverlayPos::Center:
        case OverlayPos::CenterRight:
            y = screen.top() + (screen.height() - blockH) * 0.5 + fm.ascent(); break;
        case OverlayPos::BottomLeft:
        case OverlayPos::BottomCenter:
        case OverlayPos::BottomRight:
            y = screen.bottom() - kMargin - blockH + fm.ascent(); break;
        default: break;
    }

    const QString& first = lines.first();
    qreal cursorX = x;

    if (m_s.speed && first.startsWith("Speed")) {
        const QStringList fragments = first.split(QStringLiteral(" | "));
        for (int i = 0; i < fragments.size(); ++i) {
            const QString& frag = fragments[i];
            const QColor color = (i == 0) ? kSpeedGreen : kWhite;
            drawShadowedText(p, QPointF(cursorX, y), frag, color);
            cursorX += fm.horizontalAdvance(frag);
            if (i != fragments.size() - 1) {
                const QString sep = QStringLiteral(" | ");
                drawShadowedText(p, QPointF(cursorX, y), sep, kWhite);
                cursorX += fm.horizontalAdvance(sep);
            }
        }
    } else {
        drawShadowedText(p, QPointF(x, y), first, kWhite);
    }

    for (int i = 1; i < lines.size(); ++i) {
        const qreal yy = y + lineH * i;
        drawShadowedText(p, QPointF(x, yy), lines[i], kWhite);
    }
    Q_UNUSED(kDimRed);
    Q_UNUSED(kMuted);
}

void OsdPreview::drawTopRightIndicators(QPainter& p, const QRectF& screen) const {
    QStringList items;
    if (m_s.indicators)          items << QStringLiteral("⏩ FF");
    if (m_s.videoCapture)        items << QStringLiteral("⏺ REC");
    if (m_s.inputRec)            items << QStringLiteral("● INPUT");
    if (m_s.textureReplacements) items << QStringLiteral("\U0001F3A8 TEX");
    if (items.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);

    const qreal lineH = fm.height();
    qreal y = screen.top() + kMargin + fm.ascent();
    for (const QString& t : items) {
        const qreal w = fm.horizontalAdvance(t);
        const qreal x = screen.right() - kMargin - w;
        drawShadowedText(p, QPointF(x, y), t, kWhite);
        y += lineH;
    }
}

void OsdPreview::drawBottomRightSettings(QPainter& p, const QRectF& screen) const {
    if (!m_s.settings && !m_s.patches) return;

    QStringList parts;
    if (m_s.settings)
        parts << QStringLiteral("DB=2 P=5 C=0 | CR=1 FCDVD VSYNC EER=0 EEC=1");
    if (m_s.patches)
        parts << QStringLiteral("Patches: Widescreen, NoInterlace");
    const QString text = parts.join(QStringLiteral("  "));

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(text);
    const qreal x = screen.right()  - kMargin - w;
    const qreal y = screen.bottom() - kMargin;
    drawShadowedText(p, QPointF(x, y), text, kWhite);
}

void OsdPreview::drawBottomLeftInputs(QPainter& p, const QRectF& screen) const {
    if (!m_s.inputs) return;
    const QString text =
        QStringLiteral("\U0001F3AE 1 • DualShock | A X ↑ LT:0.42");

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal x = screen.left()   + kMargin;
    const qreal y = screen.bottom() - kMargin;
    drawShadowedText(p, QPointF(x, y), text, kWhite);
}

void OsdPreview::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    QRectF client = rect().adjusted(1, 1, -1, -1);
    const qreal targetH = client.width() * 9.0 / 16.0;
    QRectF screen = client;
    if (targetH <= client.height()) {
        screen.setTop(client.top() + (client.height() - targetH) * 0.5);
        screen.setHeight(targetH);
    } else {
        const qreal targetW = client.height() * 16.0 / 9.0;
        screen.setLeft(client.left() + (client.width() - targetW) * 0.5);
        screen.setWidth(targetW);
    }

    paintGameScene(p, screen);

    p.setPen(QPen(QColor(0, 0, 0, 160), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(screen);

    drawPerfColumn(p, screen);
    drawTopRightIndicators(p, screen);
    drawBottomRightSettings(p, screen);
    drawBottomLeftInputs(p, screen);
}
