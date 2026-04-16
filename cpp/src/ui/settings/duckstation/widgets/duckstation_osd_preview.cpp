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
    // PS1-era colour palette: dark teal-purple sky, murky ground
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0,  QColor(0x28, 0x32, 0x5a));
    g.setColorAt(0.5,  QColor(0x4a, 0x38, 0x5c));
    g.setColorAt(1.0,  QColor(0x22, 0x1a, 0x28));
    p.fillRect(r, g);

    // Moon
    const qreal moonR = qMin(r.width(), r.height()) * 0.07;
    QPointF moonC(r.left() + r.width() * 0.78, r.top() + r.height() * 0.24);
    QRadialGradient mg(moonC, moonR * 1.8);
    mg.setColorAt(0.0, QColor(230, 230, 255, 210));
    mg.setColorAt(1.0, QColor(180, 180, 220, 0));
    p.setBrush(mg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(moonC, moonR * 1.8, moonR * 1.8);
}

// Top-left: FPS + Speed
void DuckStationOsdPreview::drawTopLeft(QPainter& p, const QRectF& screen) const {
    QStringList lines;
    if (m_s.fps)   lines << QStringLiteral("FPS: 59.94");
    if (m_s.speed) lines << QStringLiteral("Speed: 100%");
    if (lines.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal lineH = fm.height();
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    qreal y = screen.top() + effMargin + fm.ascent();

    for (int i = 0; i < lines.size(); ++i) {
        const QColor color = (i == 0 && m_s.fps) ? kGreen : kWhite;
        drawShadowedText(p, QPointF(screen.left() + effMargin, y + lineH * i), lines[i], color);
    }
}

// Top-right: CPU / GPU usage
void DuckStationOsdPreview::drawTopRight(QPainter& p, const QRectF& screen) const {
    QStringList lines;
    if (m_s.cpu) lines << QStringLiteral("CPU: 35.2%");
    if (m_s.gpu) lines << QStringLiteral("GPU: 48.7%");
    if (lines.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal lineH = fm.height();
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    qreal y = screen.top() + effMargin + fm.ascent();

    for (int i = 0; i < lines.size(); ++i) {
        const qreal w = fm.horizontalAdvance(lines[i]);
        const qreal x = screen.right() - effMargin - w;
        drawShadowedText(p, QPointF(x, y + lineH * i), lines[i], kYellow);
    }
}

// Bottom-left: controller inputs + status indicators
void DuckStationOsdPreview::drawBottomLeft(QPainter& p, const QRectF& screen) const {
    QStringList lines;
    if (m_s.inputs)          lines << QStringLiteral("\U0001F3AE 1 \u2022 DualShock | X O \u2191");
    if (m_s.statusIndicators) lines << QStringLiteral("\u23E9 FF  \u23FA REC");
    if (m_s.osdMessages)     lines << QStringLiteral("Loaded save state.");
    if (lines.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal lineH = fm.height();
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    const qreal blockH = lineH * lines.size();
    qreal y = screen.bottom() - effMargin - blockH + fm.ascent();

    for (int i = 0; i < lines.size(); ++i)
        drawShadowedText(p, QPointF(screen.left() + effMargin, y + lineH * i), lines[i], kWhite);
}

// Bottom-right: resolution / GPU stats / enhancements / frame times / latency
void DuckStationOsdPreview::drawBottomRight(QPainter& p, const QRectF& screen) const {
    QStringList lines;
    if (m_s.resolution)   lines << QStringLiteral("640x480 NTSC");
    if (m_s.gpuStats)     lines << QStringLiteral("GS: 3812 draws  VRAM: 384/512 MB");
    if (m_s.frameTimes)   lines << QStringLiteral("[\u2581\u2582\u2584\u2585\u2587\u2586\u2584\u2582\u2581]");
    if (m_s.latencyStats) lines << QStringLiteral("Input lat: 3.2ms  Audio lat: 46ms");
    if (m_s.enhancements) lines << QStringLiteral("Enhancements: 4x, PGXP");
    if (lines.isEmpty()) return;

    QFont f = monospaceFont(scaledFontPx());
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal lineH = fm.height();
    const qreal effMargin = kDefaultMargin + m_s.margin * 0.3;
    const qreal blockH = lineH * lines.size();
    qreal y = screen.bottom() - effMargin - blockH + fm.ascent();

    for (int i = 0; i < lines.size(); ++i) {
        const qreal w = fm.horizontalAdvance(lines[i]);
        const qreal x = screen.right() - effMargin - w;
        drawShadowedText(p, QPointF(x, y + lineH * i), lines[i], kCyan);
    }
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

    drawTopLeft(p, screen);
    drawTopRight(p, screen);
    drawBottomLeft(p, screen);
    drawBottomRight(p, screen);
}
