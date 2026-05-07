#include "controller_bindings_view.h"

#include "adapters/adapter_registry.h"
#include "adapters/emulator_adapter.h"
#include "core/sdl_input_manager.h"
#include "settings_dialog_theme.h"
#include "ui/app_controller.h"
#include "widgets/settings_card.h"

#include <QBoxLayout>
#include <QElapsedTimer>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <optional>

namespace {

// ─── Slot geometry ────────────────────────────────────────────────
//
// 5×3 QGridLayout:
//
//   ┌─────────┬───────────────────┬─────────┐
//   │ (─)     │ Shoulders strip   │ (─)     │  row 0
//   │ D-Pad   │                   │ Face    │  row 1
//   │ L-Stick │   Image area      │ R-Stick │  row 2
//   │ (─)     │ System strip      │ (─)     │  row 3
//   ├─────────┴───────────────────┴─────────┤
//   │ Footer (now-editing + face hints)      │  row 4
//   └────────────────────────────────────────┘

constexpr int kColLeft    = 0;
constexpr int kColCenter  = 1;
constexpr int kColRight   = 2;
constexpr int kRowTop     = 0;
constexpr int kRowDpadFB  = 1;
constexpr int kRowAnalogs = 2;
constexpr int kRowBottom  = 3;
constexpr int kRowFooter  = 4;

constexpr int kCardWidth    = 160;
constexpr int kImageMinW    = 480;
constexpr int kImageMinH    = 360;
constexpr int kFooterHeight = 130;

QString sectionHeaderText(const QString& slot) {
    if (slot == "DPad")         return "D-PAD";
    if (slot == "FaceButtons")  return "FACE BUTTONS";
    if (slot == "LeftAnalog")   return "LEFT ANALOG";
    if (slot == "RightAnalog")  return "RIGHT ANALOG";
    if (slot == "Shoulders")    return "SHOULDERS";
    if (slot == "System")       return "SYSTEM";
    return slot.toUpper();
}

QString resolveSlot(const BindingDef& b) {
    if (!b.cardSlot.isEmpty()) return b.cardSlot;
    QString g = b.group;
    g.remove(' ');
    if (g.compare("dpad",         Qt::CaseInsensitive) == 0) return "DPad";
    if (g.compare("facebuttons",  Qt::CaseInsensitive) == 0) return "FaceButtons";
    if (g.compare("leftanalog",   Qt::CaseInsensitive) == 0
     || g.compare("leftstick",    Qt::CaseInsensitive) == 0) return "LeftAnalog";
    if (g.compare("rightanalog",  Qt::CaseInsensitive) == 0
     || g.compare("rightstick",   Qt::CaseInsensitive) == 0) return "RightAnalog";
    if (g.compare("shoulders",    Qt::CaseInsensitive) == 0
     || g.compare("triggers",     Qt::CaseInsensitive) == 0) return "Shoulders";
    return "System";
}

} // namespace

// ─── BindingCard — SettingsCard that carries a BindingDef ────────────

class ControllerBindingsView::BindingCard : public SettingsCard {
    Q_OBJECT
public:
    explicit BindingCard(const BindingDef& def, QWidget* parent = nullptr)
        : SettingsCard(parent), m_def(def) {
        pinToReferenceHeight();
        setFixedWidth(kCardWidth);
        setStyleSheet(SettingsDialogTheme::cardQss());

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(10, 6, 10, 6);
        lay->setSpacing(2);

        m_label = new QLabel(def.label.toUpper(), this);
        m_label->setStyleSheet(QStringLiteral(
            "color: #d0ccc4; font-size: 9px; font-weight: 600;"
            "letter-spacing: 1.4px; background: transparent;"));

        m_value = new QLabel("Not bound", this);
        m_value->setStyleSheet(QStringLiteral(
            "color: #f2efe8; font-size: 13px; background: transparent;"));

        lay->addWidget(m_label);
        lay->addWidget(m_value);
    }

    const BindingDef& def() const { return m_def; }

    void setCurrentValue(const QString& v) {
        if (v.isEmpty()) {
            m_value->setText("Not bound");
            m_value->setStyleSheet(QStringLiteral(
                "color: #9a9690; font-size: 13px; font-style: italic;"
                "background: transparent;"));
        } else {
            m_value->setText(v);
            m_value->setStyleSheet(QStringLiteral(
                "color: #f2efe8; font-size: 13px; background: transparent;"));
        }
    }

    QString currentText() const { return m_value->text(); }

private:
    BindingDef m_def;
    QLabel* m_label;
    QLabel* m_value;
};

// ─── ImageArea — controller SVG + dim overlay + amber pulse ring ─────

class ControllerBindingsView::ImageArea : public QWidget {
    Q_OBJECT
public:
    explicit ImageArea(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(kImageMinW, kImageMinH);
        m_pulseTimer.setInterval(33);   // ~30 fps
        connect(&m_pulseTimer, &QTimer::timeout, this, [this](){ update(); });
        m_pulseClock.start();
    }

    void setControllerSvg(const QString& resourcePath) {
        m_renderer.load(resourcePath);
        if (m_renderer.isValid()) m_viewBox = m_renderer.viewBoxF();
        update();
    }

    void setFocusedBinding(const BindingDef* b) {
        if (!b || b->spotlightR == 0) {
            m_focused.reset();
            m_pulseTimer.stop();
        } else {
            m_focused = *b;
            m_pulseTimer.start();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor("#585450"));

        if (!m_renderer.isValid()) return;

        const QSizeF vb = m_viewBox.size();
        if (vb.width() <= 0 || vb.height() <= 0) return;
        const qreal scale = std::min(width() / vb.width(), height() / vb.height());
        const qreal renderW = vb.width()  * scale;
        const qreal renderH = vb.height() * scale;
        const qreal originX = (width()  - renderW) / 2.0;
        const qreal originY = (height() - renderH) / 2.0;
        const QRectF imageRect(originX, originY, renderW, renderH);

        m_renderer.render(&p, imageRect);

        if (!m_focused.has_value() || m_focused->spotlightR == 0) return;

        // Spotlight: dark overlay with radial-gradient cutout.
        const qreal cx = originX + m_focused->spotlightX * scale;
        const qreal cy = originY + m_focused->spotlightY * scale;
        const qreal r  = m_focused->spotlightR * scale;

        QRadialGradient grad(QPointF(cx, cy), r * 1.5);
        grad.setColorAt(0.00, QColor(0, 0, 0,   0));
        grad.setColorAt(0.55, QColor(0, 0, 0,   0));
        grad.setColorAt(1.00, QColor(0, 0, 0, 158));   // ~62% alpha
        p.fillRect(imageRect, grad);

        // Amber pulse ring on top.
        const qreal phaseT = std::sin(m_pulseClock.elapsed() / 254.6) * 0.5 + 0.5;
        const qreal ringR = r * 1.4 + phaseT * 2.0;
        const int ringAlpha = 217 + int(phaseT * 38);

        // Glow layer.
        QPen glowPen(QColor(245, 158, 11, 96), 8);
        p.setPen(glowPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(cx, cy), ringR, ringR);

        // Sharp ring.
        QPen ringPen(QColor(245, 158, 11, ringAlpha), 3);
        p.setPen(ringPen);
        p.drawEllipse(QPointF(cx, cy), ringR, ringR);
    }

private:
    QSvgRenderer              m_renderer;
    QRectF                    m_viewBox{0, 0, 1000, 1000};
    std::optional<BindingDef> m_focused;
    QTimer                    m_pulseTimer;
    QElapsedTimer             m_pulseClock;
};

// ─── ControllerBindingsView ─────────────────────────────────────────

ControllerBindingsView::ControllerBindingsView(SdlInputManager* inputManager,
                                                AppController* appController,
                                                const QString& emuId,
                                                int port,
                                                QWidget* parent)
    : QWidget(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_port(port)
{
    setStyleSheet(QStringLiteral("background: #585450;"));

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    Q_ASSERT(adapter);
    const auto types = adapter->controllerTypes();
    Q_ASSERT_X(types.size() == 1, "ControllerBindingsView",
               "Adapter must declare exactly one controller type for the new view.");

    const QString typeId = types.front().id;
    const QString svg    = types.front().svgResource;
    m_bindings = adapter->controllerBindingDefsForType(typeId);

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(20, 16, 20, 0);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(kColLeft,   0);
    grid->setColumnStretch(kColCenter, 1);
    grid->setColumnStretch(kColRight,  0);
    grid->setRowStretch(kRowAnalogs, 1);

    m_imageArea = new ImageArea(this);
    m_imageArea->setControllerSvg(svg);
    grid->addWidget(m_imageArea, kRowTop, kColCenter, kRowBottom - kRowTop + 1, 1);

    buildSlots(m_bindings);

    // Footer.
    auto* footer = new QFrame(this);
    footer->setFixedHeight(kFooterHeight);
    footer->setStyleSheet(QStringLiteral(
        "background: #4a4642; border-left: 3px solid #f59e0b;"));

    auto* footerLay = new QHBoxLayout(footer);
    footerLay->setContentsMargins(28, 0, 28, 0);
    footerLay->setSpacing(24);

    auto* leftBlock = new QWidget(footer);
    auto* leftV = new QVBoxLayout(leftBlock);
    leftV->setContentsMargins(0, 0, 0, 0);
    leftV->setSpacing(6);

    m_nowLabel = new QLabel("READY", leftBlock);
    m_nowLabel->setStyleSheet(QStringLiteral(
        "color: #d0ccc4; font-size: 10px; letter-spacing: 3px;"
        "background: transparent;"));

    m_nowValue = new QLabel({}, leftBlock);
    m_nowValue->setStyleSheet(QStringLiteral(
        "color: #f2efe8; font-size: 30px; font-weight: 300;"
        "letter-spacing: 1.5px; background: transparent;"));

    leftV->addWidget(m_nowLabel);
    leftV->addWidget(m_nowValue);
    footerLay->addWidget(leftBlock, 1);

    struct FaceHint { const char* label; const char* face; const char* bg; const char* fg; };
    const FaceHint hints[] = {
        {"Rebind",   "A", "#39c46b", "#0e2a14"},
        {"Clear",    "B", "#e74c4c", "#2a0e0e"},
        {"Auto-Map", "Y", "#ffd23a", "#2a210e"},
        {"Close",    "X", "#3aa6ff", "#0e1f2a"},
    };
    auto* hintsRow = new QHBoxLayout();
    hintsRow->setSpacing(24);
    for (const auto& h : hints) {
        auto* row = new QHBoxLayout();
        row->setSpacing(9);
        auto* face = new QLabel(h.face, footer);
        face->setFixedSize(28, 28);
        face->setAlignment(Qt::AlignCenter);
        face->setStyleSheet(QString(
            "background: %1; color: %2; border-radius: 14px;"
            "font-size: 13px; font-weight: 800;").arg(h.bg, h.fg));
        auto* lbl = new QLabel(h.label, footer);
        lbl->setStyleSheet(QStringLiteral(
            "color: #f2efe8; font-size: 13px; background: transparent;"));
        row->addWidget(face);
        row->addWidget(lbl);
        hintsRow->addLayout(row);
    }
    footerLay->addLayout(hintsRow, 0);

    grid->addWidget(footer, kRowFooter, kColLeft, 1, 3);

    reloadBindings();
}

ControllerBindingsView::~ControllerBindingsView() = default;

void ControllerBindingsView::buildSlots(const QVector<BindingDef>& bindings) {
    auto* grid = qobject_cast<QGridLayout*>(layout());
    Q_ASSERT(grid);

    struct SlotPos { int row; int col; };
    static const QHash<QString, SlotPos> slotPositions{
        {"DPad",         {kRowDpadFB,  kColLeft}},
        {"LeftAnalog",   {kRowAnalogs, kColLeft}},
        {"FaceButtons",  {kRowDpadFB,  kColRight}},
        {"RightAnalog",  {kRowAnalogs, kColRight}},
        {"Shoulders",    {kRowTop,     kColCenter}},
        {"System",       {kRowBottom,  kColCenter}},
    };

    QHash<QString, QVector<BindingDef>> bySlot;
    QVector<QString> slotOrder;
    for (const auto& b : bindings) {
        const QString slot = resolveSlot(b);
        if (!bySlot.contains(slot)) slotOrder.append(slot);
        bySlot[slot].append(b);
    }

    for (const QString& slot : slotOrder) {
        const auto pos = slotPositions.value(slot, SlotPos{kRowBottom, kColCenter});

        auto* container = new QWidget(this);
        container->setStyleSheet("background: transparent;");
        auto* v = new QVBoxLayout(container);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(6);

        auto* header = new QLabel(sectionHeaderText(slot), container);
        header->setStyleSheet(QStringLiteral(
            "color: #f59e0b; font-size: 10px; font-weight: 600;"
            "letter-spacing: 1.8px; background: transparent;"));
        v->addWidget(header);

        const bool horizontal = (slot == "Shoulders" || slot == "System");
        QBoxLayout* cardLay = horizontal
            ? static_cast<QBoxLayout*>(new QHBoxLayout())
            : static_cast<QBoxLayout*>(new QVBoxLayout());
        cardLay->setSpacing(8);
        cardLay->setContentsMargins(0, 0, 0, 0);

        for (const auto& b : bySlot[slot]) {
            // Skip bindings with no INI key (kept in the schema for save/restore
            // but have no card on the page — e.g. some legacy abstract entries).
            if (b.key.isEmpty()) continue;
            auto* card = new BindingCard(b, container);
            cardLay->addWidget(card);
            card->installEventFilter(this);
            connect(card, &SettingsCard::focused, this, [this, card](const SettingDef&){
                onCardFocused(card->def());
            });
            connect(card, &SettingsCard::activated, this, [this, card](){
                emit rebindRequested(card->def());
            });
        }
        if (horizontal) cardLay->addStretch(1);
        v->addLayout(cardLay);
        if (!horizontal) v->addStretch(1);

        grid->addWidget(container, pos.row, pos.col);
    }
}

void ControllerBindingsView::reloadBindings() {
    if (m_appController) {
        const QVariantList raw = m_appController->controllerBindingsForPort(m_emuId, m_port);
        m_currentValues.clear();
        for (const auto& v : raw) {
            const auto map = v.toMap();
            m_currentValues.insert(map.value("key").toString(),
                                    map.value("currentValue").toString());
        }
    }

    const auto cards = findChildren<BindingCard*>();
    for (auto* card : cards)
        card->setCurrentValue(m_currentValues.value(card->def().key));
}

void ControllerBindingsView::onCardFocused(const BindingDef& b) {
    m_imageArea->setFocusedBinding(&b);
    updateNowEditing(b, currentValueFor(b.key));
    emit bindingFocused(b);
}

void ControllerBindingsView::updateNowEditing(const BindingDef& b, const QString& value) {
    m_nowLabel->setText("NOW EDITING");
    const QString display = value.isEmpty() ? QStringLiteral("Not bound") : value;
    m_nowValue->setText(QString(
        "%1  <span style='color:#f59e0b;'>→</span>  "
        "<span style='color:#f59e0b; font-weight:600;'>%2</span>")
        .arg(b.label.toHtmlEscaped(), display.toHtmlEscaped()));
    m_nowValue->setTextFormat(Qt::RichText);
}

QString ControllerBindingsView::currentValueFor(const QString& key) const {
    return m_currentValues.value(key);
}

bool ControllerBindingsView::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* card = qobject_cast<BindingCard*>(watched);
        if (!card || !card->hasFocus()) return QWidget::eventFilter(watched, event);

        const auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Backspace || ke->key() == Qt::Key_B) {
            emit clearRequested(card->def());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// Inner classes defined in this .cpp file carry Q_OBJECT, so their moc
// output must be included here (AUTOMOC only processes headers).
#include "controller_bindings_view.moc"
