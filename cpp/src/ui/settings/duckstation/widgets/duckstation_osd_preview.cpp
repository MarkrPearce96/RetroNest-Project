#include "duckstation_osd_preview.h"
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
constexpr qreal kDefaultMargin = 8.0;

QColor kShadow (0,    0,    0,   217);
QColor kWhite  (0xff, 0xff, 0xff);
QColor kGreen  (0x60, 0xff, 0x60);
QColor kYellow (0xff, 0xe0, 0x60);
QColor kCyan   (0x60, 0xe8, 0xff);

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
} // namespace

DuckStationOsdPreview::DuckStationOsdPreview(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void DuckStationOsdPreview::setShowFPS(bool on)              { m_s.fps = on;            update(); }
void DuckStationOsdPreview::setShowSpeed(bool on)            { m_s.speed = on;          update(); }
void DuckStationOsdPreview::setShowCPU(bool on)              { m_s.cpu = on;            update(); }
void DuckStationOsdPreview::setShowGPU(bool on)              { m_s.gpu = on;            update(); }
void DuckStationOsdPreview::setShowResolution(bool on)       { m_s.resolution = on;     update(); }
void DuckStationOsdPreview::setShowGPUStatistics(bool on)    { m_s.gpuStats = on;       update(); }
void DuckStationOsdPreview::setShowFrameTimes(bool on)       { m_s.frameTimes = on;     update(); }
void DuckStationOsdPreview::setShowLatencyStatistics(bool on){ m_s.latencyStats = on;   update(); }
void DuckStationOsdPreview::setShowInputs(bool on)           { m_s.inputs = on;         update(); }
void DuckStationOsdPreview::setShowEnhancements(bool on)     { m_s.enhancements = on;   update(); }
void DuckStationOsdPreview::setShowOSDMessages(bool on)      { m_s.osdMessages = on;    update(); }
void DuckStationOsdPreview::setShowStatusIndicators(bool on) { m_s.statusIndicators = on; update(); }

void DuckStationOsdPreview::setMessageLocation(const QString& loc) {
    m_s.messageLocation = loc;
    update();
}

void DuckStationOsdPreview::setScale(int percent) {
    m_s.scale = std::clamp(percent, 50, 500);
    update();
}

void DuckStationOsdPreview::setMargin(int pixels) {
    m_s.margin = std::clamp(pixels, 0, 100);
    update();
}

int DuckStationOsdPreview::scaledFontPx() const {
    const int px = int(qRound(10.0 * double(m_s.scale) / 100.0));
    return std::clamp(px, 6, 24);
}

void DuckStationOsdPreview::paintGameScene(QPainter& p, const QRectF& r) const {
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0,  QColor(0x28, 0x32, 0x5a));
    g.setColorAt(0.5,  QColor(0x4a, 0x38, 0x5c));
    g.setColorAt(1.0,  QColor(0x22, 0x1a, 0x28));
    p.fillRect(r, g);

    const qreal moonR = qMin(r.width(), r.height()) * 0.07;
    QPointF moonC(r.left() + r.width() * 0.78, r.top() + r.height() * 0.24);
    QRadialGradient mg(moonC, moonR * 1.8);
    mg.setColorAt(0.0, QColor(230, 230, 255, 210));
    mg.setColorAt(1.0, QColor(180, 180, 220, 0));
    p.setBrush(mg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(moonC, moonR * 1.8, moonR * 1.8);
}

// Top-right stack: every performance stat plus the status indicators, in the
// same order DuckStation's DrawPerformanceOverlay emits them. All lines are
// right-anchored against `screen.right() - margin`.
void DuckStationOsdPreview::drawTopRightStack(QPainter& p, const QRectF& screen) const {
    struct Line { QString text; QColor color; };
    QList<Line> lines;
    if (m_s.fps || m_s.speed) {
        QStringList parts;
        if (m_s.fps)   parts << QStringLiteral("59.94 FPS");
        if (m_s.speed) parts << QStringLiteral("100% Speed");
        lines.push_back({parts.join(QStringLiteral(" | ")), kGreen});
    }
    if (m_s.cpu)          lines.push_back({QStringLiteral("CPU: 35.2% (5.87ms)"),       kYellow});
    if (m_s.gpu)          lines.push_back({QStringLiteral("GPU: 48.7% (8.13ms)"),       kYellow});
    if (m_s.resolution)   lines.push_back({QStringLiteral("640x480 NTSC"),              kCyan});
    if (m_s.gpuStats)     lines.push_back({QStringLiteral("3812 draws  VRAM 12/512"),   kCyan});
    if (m_s.frameTimes)   lines.push_back({QStringLiteral("[▁▂▄▅▇▆▄▂▁]"), kCyan});
    if (m_s.latencyStats) lines.push_back({QStringLiteral("Input 3.2ms  Audio 46ms"),   kCyan});
    if (m_s.statusIndicators) lines.push_back({QStringLiteral("⏩ FF  ⏺ REC"), kWhite});
    if (lines.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal lineH = fm.height();
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    qreal y = screen.top() + effMargin + fm.ascent();

    for (int i = 0; i < lines.size(); ++i) {
        const Line& ln = lines[i];
        const qreal w = fm.horizontalAdvance(ln.text);
        const qreal x = screen.right() - effMargin - w;
        drawShadowedText(p, QPointF(x, y + lineH * i), ln.text, ln.color);
    }
}

// Bottom-left: controller inputs only (matches DrawInputsOverlay).
void DuckStationOsdPreview::drawBottomLeftInputs(QPainter& p, const QRectF& screen) const {
    if (!m_s.inputs) return;
    const QString text = QStringLiteral("\U0001F3AE 1 • DualShock | X O ↑");

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    const qreal y = screen.bottom() - effMargin;
    drawShadowedText(p, QPointF(screen.left() + effMargin, y), text, kWhite);
}

// Bottom-right: enhancements summary (matches DrawEnhancementsOverlay).
void DuckStationOsdPreview::drawBottomRightEnhancements(QPainter& p, const QRectF& screen) const {
    if (!m_s.enhancements) return;
    const QString text = QStringLiteral("NTSC HW DI=Weave IR=4x PGXP");

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    const qreal w = fm.horizontalAdvance(text);
    const qreal x = screen.right() - effMargin - w;
    const qreal y = screen.bottom() - effMargin;
    drawShadowedText(p, QPointF(x, y), text, kCyan);
}

// OSD messages — anchor depends on the OSDMessageLocation combo. Default Top
// Left. Drawn last so it overlays the corner stacks if positions overlap.
void DuckStationOsdPreview::drawOsdMessage(QPainter& p, const QRectF& screen) const {
    if (!m_s.osdMessages) return;
    const QString text = QStringLiteral("Loaded save state.");

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    const qreal w = fm.horizontalAdvance(text);

    const bool isTop    = m_s.messageLocation.startsWith(QStringLiteral("Top"),    Qt::CaseInsensitive);
    const bool isCenter = m_s.messageLocation.contains  (QStringLiteral("Center"), Qt::CaseInsensitive);
    const bool isRight  = m_s.messageLocation.contains  (QStringLiteral("Right"),  Qt::CaseInsensitive);

    qreal x;
    if (isCenter)     x = screen.left() + (screen.width() - w) * 0.5;
    else if (isRight) x = screen.right() - effMargin - w;
    else              x = screen.left() + effMargin;

    qreal y;
    if (isTop) y = screen.top() + effMargin + fm.ascent();
    else       y = screen.bottom() - effMargin;

    drawShadowedText(p, QPointF(x, y), text, kWhite);
}

void DuckStationOsdPreview::paintEvent(QPaintEvent*) {
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

    drawTopRightStack(p, screen);
    drawBottomLeftInputs(p, screen);
    drawBottomRightEnhancements(p, screen);
    drawOsdMessage(p, screen);
}
