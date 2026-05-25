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
#include <QResizeEvent>
#include <QScrollArea>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <limits>
#include <optional>

namespace {

// ─── Slot geometry ────────────────────────────────────────────────
//
// 5×3 QGridLayout:
//
//   ┌─────────┬───────────────────┬─────────┐
//   │ (─)     │ [L2|L1] ← → [R1|R2]       │  row 0  ← shoulder row (center col only)
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

constexpr int kCardHeightInline    = 36;   // label-left + value-right
constexpr int kCardHeightStacked   = 56;   // label-top + value-below
constexpr int kCardMinWidth        = 140;  // inline cards shrink to this in horizontal strips
constexpr int kCardStackedWidth    = 110;  // each stacked card in a horizontal strip
constexpr int kColumnWidth         = 220;  // fixed width for vertical-stack columns
constexpr int kImageMinW    = 480;
constexpr int kImageMinH    = 340;   // 974/665 ≈ 1.46 aspect → 480/1.46 ≈ 329
constexpr int kFooterHeight = 130;

QString sectionHeaderText(const QString& slot,
                          const QHash<QString, QString>& overrides = {}) {
    if (auto it = overrides.constFind(slot); it != overrides.constEnd())
        return it.value();
    if (slot == "DPad")             return "D-PAD";
    if (slot == "FaceButtons")      return "FACE BUTTONS";
    if (slot == "LeftAnalog")       return "LEFT ANALOG";
    if (slot == "RightAnalog")      return "RIGHT ANALOG";
    if (slot == "Shoulders")        return "SHOULDERS";
    if (slot == "LeftShoulders")    return "SHOULDERS";
    if (slot == "RightShoulders")   return "SHOULDERS";
    if (slot == "System")           return "SYSTEM";
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
    enum class Style { Inline, Stacked };

    BindingCard(const BindingDef& def, Style style, QWidget* parent = nullptr)
        : SettingsCard(parent), m_def(def), m_style(style) {
        setObjectName("ControllerBindingCard");
        setFixedHeight(style == Style::Inline ? kCardHeightInline : kCardHeightStacked);
        if (style == Style::Inline) setMinimumWidth(kCardMinWidth);
        else                        setFixedWidth(kCardStackedWidth);
        setStyleSheet(QStringLiteral(
            "QFrame#ControllerBindingCard {"
            "  background-color: #4a4642;"
            "  border: 1px solid #706c66;"
            "  border-radius: 8px;"
            "}"
            "QFrame#ControllerBindingCard[focused=\"true\"] {"
            "  border: 1px solid #f59e0b;"
            "}"));

        m_label = new QLabel(def.label.toUpper(), this);
        m_label->setStyleSheet(QStringLiteral(
            "color: #d0ccc4; font-size: 9px; font-weight: 600;"
            "letter-spacing: 1.4px; background: transparent;"));

        m_value = new QLabel("Not bound", this);
        m_value->setStyleSheet(QStringLiteral(
            "color: #f2efe8; font-size: 12px; background: transparent;"));

        if (style == Style::Inline) {
            auto* lay = new QHBoxLayout(this);
            lay->setContentsMargins(12, 4, 12, 4);
            lay->setSpacing(10);
            m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            lay->addWidget(m_label);
            lay->addStretch(1);
            lay->addWidget(m_value);
        } else {  // Stacked
            auto* lay = new QVBoxLayout(this);
            lay->setContentsMargins(12, 6, 12, 6);
            lay->setSpacing(2);
            m_value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            lay->addWidget(m_label);
            lay->addWidget(m_value);
        }
    }

    const BindingDef& def() const { return m_def; }

    void setCurrentValue(const QString& v) {
        if (v.isEmpty()) {
            m_value->setText("Not bound");
            m_value->setStyleSheet(QStringLiteral(
                "color: #9a9690; font-size: 12px; font-style: italic;"
                "background: transparent;"));
        } else {
            m_value->setText(v);
            m_value->setStyleSheet(QStringLiteral(
                "color: #f2efe8; font-size: 12px; background: transparent;"));
        }
    }

    /// Show the "Press a button…" italic amber state while the dialog
    /// captures the next button press for this binding. The next call
    /// to `setCurrentValue` (driven by the view's `reloadBindings()`)
    /// restores the real value display.
    void setCapturingState(bool capturing) {
        if (capturing) {
            m_value->setText("Press a button…");
            m_value->setStyleSheet(QStringLiteral(
                "color: #f59e0b; font-size: 12px; font-style: italic;"
                "font-weight: 600; background: transparent;"));
        }
        // When clearing, leave the current text — reloadBindings will
        // overwrite it with the saved binding moments later.
    }

    QString currentText() const { return m_value->text(); }

signals:
    /// Emitted when the user requests a clear on this card (controller-aware).
    void clearRequested(const BindingDef& b);

protected:
    void keyPressEvent(QKeyEvent* e) override {
        // Walk up to the view to check capture state.
        QWidget* w = parentWidget();
        ControllerBindingsView* view = nullptr;
        while (w && !(view = qobject_cast<ControllerBindingsView*>(w)))
            w = w->parentWidget();

        // While the dialog is in capture mode, the SdlInputManager will
        // emit bindingCaptured() for the next physical button press. The
        // card's own arrow / activate / clear handling would otherwise
        // move focus or re-trigger activate — which causes the capture
        // to never see the press. Be silent until capture finishes.
        if (view && view->isCapturing()) {
            e->accept();
            return;
        }

        const int k = e->key();

        // ── Arrow keys: spatial navigation ───────────────────────────────
        if (k == Qt::Key_Left || k == Qt::Key_Right ||
            k == Qt::Key_Up   || k == Qt::Key_Down) {

            // View-wide spatial navigation. Walk up to the topmost ControllerBindingsView
            // ancestor and consider every BindingCard inside it as a candidate.
            QWidget* root = this;
            while (root->parentWidget()) root = root->parentWidget();

            const auto allCards = root->findChildren<BindingCard*>();
            if (allCards.size() < 2) {
                SettingsCard::keyPressEvent(e);
                return;
            }

            const QPoint myCenter = mapTo(root, rect().center());
            const QRect  myRect   = QRect(mapTo(root, QPoint(0, 0)), size());

            BindingCard* best = nullptr;
            long long bestScore = std::numeric_limits<long long>::max();

            auto rangesOverlap = [](int a0, int a1, int b0, int b1) {
                return a0 < b1 && b0 < a1;
            };

            const bool vertical = (k == Qt::Key_Up || k == Qt::Key_Down);

            for (BindingCard* s : allCards) {
                if (s == this || !s->isVisible()) continue;

                const QPoint c   = s->mapTo(root, s->rect().center());
                const QRect  r   = QRect(s->mapTo(root, QPoint(0, 0)), s->size());
                const int    dx  = c.x() - myCenter.x();
                const int    dy  = c.y() - myCenter.y();

                bool inDir = false;
                switch (k) {
                    case Qt::Key_Left:  inDir = dx < 0; break;
                    case Qt::Key_Right: inDir = dx > 0; break;
                    case Qt::Key_Up:    inDir = dy < 0; break;
                    case Qt::Key_Down:  inDir = dy > 0; break;
                }
                if (!inDir) continue;

                // Prefer candidates whose perpendicular extent overlaps ours
                // (favours straight-line travel).
                const bool perpOverlap = vertical
                    ? rangesOverlap(myRect.left(),  myRect.right(),  r.left(),  r.right())
                    : rangesOverlap(myRect.top(),   myRect.bottom(), r.top(),   r.bottom());

                const long long adx = std::abs(static_cast<long long>(dx));
                const long long ady = std::abs(static_cast<long long>(dy));
                // Distance score weighted by direction; perpendicular overlap
                // gets a steep bonus so off-axis cards aren't preferred.
                long long score = vertical ? (ady * 2 + adx) : (adx * 2 + ady);
                if (!perpOverlap) score += 10000;  // strongly de-prioritise off-axis
                if (score < bestScore) {
                    bestScore = score;
                    best = s;
                }
            }

            if (best) {
                best->setFocus(Qt::TabFocusReason);
                // Scroll the new focus into view if inside a scroll area.
                QWidget* p = best->parentWidget();
                while (p) {
                    if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                        sa->ensureWidgetVisible(best, 20, 40);
                        break;
                    }
                    p = p->parentWidget();
                }
                e->accept();
            } else {
                SettingsCard::keyPressEvent(e);
            }
            return;
        }

        // Activate (Rebind) and Clear key handling. SDL maps both
        // Xbox-A and PS-Cross to SDL_BUTTON_A → Key_Return, and
        // Xbox-B / PS-Circle to SDL_BUTTON_B → Key_Back. So Cross
        // naturally triggers rebind and Circle triggers clear with
        // no per-controller swap.
        if (k == Qt::Key_Return || k == Qt::Key_Enter) {
            // Emit directly — SettingsCard's base only emits when
            // Key_Return / Key_Enter is pressed, but BindingCard has
            // no inner combo/slider/spinbox so the signal is what we
            // want either way.
            emit activated();
            e->accept();
            return;
        }
        // Clear:
        //  - Key_Back: only source is Circle (B) injection → always clear
        //  - Key_Backspace: keyboard Backspace clears, but synthetic
        //    Backspace from Square (X) injection should fall through to
        //    the dialog's Close shortcut, so gate on spontaneous().
        if (k == Qt::Key_Back ||
            (k == Qt::Key_Backspace && e->spontaneous())) {
            emit clearRequested(m_def);
            e->accept();
            return;
        }

        // Anything else → SettingsCard's default handler.
        SettingsCard::keyPressEvent(e);
    }

    bool event(QEvent* e) override {
        // Veto dialog-level QShortcut activation for keys we handle in
        // keyPressEvent. Without this, Qt fires QShortcut(Backspace) =
        // Close before our clear handler runs, so Backspace on a focused
        // card closes the dialog instead of clearing the binding.
        if (e->type() == QEvent::ShortcutOverride) {
            auto* ke = static_cast<QKeyEvent*>(e);
            const int k = ke->key();

            // Backspace: only veto for real keyboard presses. Square (X)
            // injects a synthetic Qt::Key_Backspace — that's non-spontaneous
            // and should fall through to the dialog's Backspace shortcut
            // so the controller user can exit the mapping page.
            // Key_Back (Circle) always clears, so we always veto it here.
            if (k == Qt::Key_Backspace && !ke->spontaneous())
                return SettingsCard::event(e);

            // Same set of keys keyPressEvent claims — keep in sync.
            if (k == Qt::Key_Backspace ||
                k == Qt::Key_Back      ||
                k == Qt::Key_Return    ||
                k == Qt::Key_Enter     ||
                k == Qt::Key_Up        ||
                k == Qt::Key_Down      ||
                k == Qt::Key_Left      ||
                k == Qt::Key_Right) {
                ke->accept();
                return true;   // suppress shortcut, let keyPressEvent run
            }
        }
        return SettingsCard::event(e);
    }

private:
    BindingDef m_def;
    Style      m_style;
    QLabel*    m_label;
    QLabel*    m_value;
};

// ─── ImageArea — controller SVG + amber highlight + pulse ring ───────

class ControllerBindingsView::ImageArea : public QWidget {
    Q_OBJECT
public:
    explicit ImageArea(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumSize(kImageMinW, kImageMinH);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_pulseTimer.setInterval(33);   // ~30 fps
        connect(&m_pulseTimer, &QTimer::timeout, this, [this](){ update(); });
        m_pulseClock.start();
    }

    void setControllerSvg(const QString& resourcePath) {
        m_renderer.load(resourcePath);
        if (m_renderer.isValid()) m_viewBox = m_renderer.viewBoxF();
        update();
    }

    qreal aspectRatio() const {
        const QSizeF vb = m_viewBox.size();
        if (vb.width() <= 0 || vb.height() <= 0) return 1.46;
        return vb.width() / vb.height();
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

        // 1. SVG.
        m_renderer.render(&p, imageRect);

        if (!m_focused.has_value() || m_focused->spotlightR == 0) return;

        // 2. Spotlight target in widget coords.
        const qreal cx = originX + m_focused->spotlightX * scale;
        const qreal cy = originY + m_focused->spotlightY * scale;
        const qreal r  = m_focused->spotlightR * scale;

        // 3. Subtle amber-tinted highlight ON the focused button (no dimming
        //    elsewhere — the controller stays bright at full brightness,
        //    OpenEmu-style).
        QRadialGradient highlight(QPointF(cx, cy), r * 1.4);
        highlight.setColorAt(0.00, QColor(245, 158, 11, 70));   // ~28% amber at center
        highlight.setColorAt(0.70, QColor(245, 158, 11, 30));   // ~12% amber midway
        highlight.setColorAt(1.00, QColor(245, 158, 11,  0));   // fully transparent at edge
        p.setBrush(highlight);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(cx, cy), r * 1.4, r * 1.4);

        // 4. Amber pulse ring on top.
        const qreal phaseT = std::sin(m_pulseClock.elapsed() / 254.6) * 0.5 + 0.5;
        const qreal ringR = r * 1.2 + phaseT * 2.0;
        const int ringAlpha = 217 + int(phaseT * 38);

        // Glow layer (wider, semi-transparent).
        QPen glowPen(QColor(245, 158, 11, 96), 8);
        p.setPen(glowPen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(cx, cy), ringR, ringR);

        // Sharp ring on top.
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
                                                const QString& controllerTypeId,
                                                int port,
                                                QWidget* parent)
    : QWidget(parent)
    , m_inputManager(inputManager)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_controllerTypeId(controllerTypeId)
    , m_port(port)
{
    setStyleSheet(QStringLiteral("background: #585450;"));

    auto* adapter = AdapterRegistry::instance().adapterFor(emuId);
    Q_ASSERT(adapter);

    const auto adapterTypes = adapter->controllerTypes();

    // Find the requested type. If the caller passed an empty id (legacy
    // single-type adapters), pick the first entry — preserves prior behavior.
    const ControllerTypeDef* matchedType = nullptr;
    if (controllerTypeId.isEmpty() && !adapterTypes.isEmpty()) {
        matchedType = &adapterTypes.first();
    } else {
        for (const auto& t : adapterTypes) {
            if (t.id == controllerTypeId) {
                matchedType = &t;
                break;
            }
        }
    }
    Q_ASSERT_X(matchedType != nullptr, "ControllerBindingsView",
               qPrintable(QString("controller type '%1' not found in adapter list for '%2'")
                          .arg(controllerTypeId, emuId)));

    // Release builds compile Q_ASSERT_X out, so guard the null explicitly: an
    // unknown controllerTypeId (e.g. a QML id that drifted from the adapter's
    // controllerTypes()) must degrade gracefully, not dereference null below.
    if (!matchedType) {
        if (!adapterTypes.isEmpty()) {
            qWarning() << "[ControllerBindingsView] controller type" << controllerTypeId
                       << "not found for" << emuId << "— falling back to" << adapterTypes.first().id;
            matchedType = &adapterTypes.first();
        } else {
            qWarning() << "[ControllerBindingsView] no controller types for" << emuId
                       << "— rendering empty view";
            return;
        }
    }

    const QString svg    = matchedType->svgResource;
    m_slotTitleOverrides = matchedType->slotTitleOverrides;
    const QString typeId = matchedType->id;
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
    grid->addWidget(m_imageArea, kRowDpadFB, kColCenter, kRowBottom - kRowDpadFB + 1, 1, Qt::AlignCenter);

    // ── Shoulder-pair top row ─────────────────────────────────────────
    // The L2/L1 (LeftShoulders) and R1/R2 (RightShoulders) clusters
    // both live in the top of the center column, side-by-side with
    // a stretch between them — so the cards visually sit above the
    // L1/L2 and R1/R2 buttons on the controller artwork below.
    auto* shoulderRow = new QWidget(this);
    shoulderRow->setStyleSheet("background: transparent;");
    m_shoulderRowLayout = new QHBoxLayout(shoulderRow);
    m_shoulderRowLayout->setContentsMargins(0, 0, 0, 0);
    m_shoulderRowLayout->setSpacing(20);
    grid->addWidget(shoulderRow, kRowTop, kColCenter, Qt::AlignTop);

    buildSlots(m_bindings);

    // Footer.
    auto* footer = new QFrame(this);
    footer->setObjectName("ControllerBindingsFooter");
    footer->setAttribute(Qt::WA_StyledBackground, true);
    footer->setFixedHeight(kFooterHeight);
    footer->setStyleSheet(QStringLiteral(
        "QFrame#ControllerBindingsFooter {"
        "  background: #4a4642;"
        "  border-left: 3px solid #f59e0b;"
        "}"));

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

    auto* hintsRow = new QHBoxLayout();
    hintsRow->setSpacing(24);

    auto buildHintCell = [&](FaceHintWidget& hint, const QString& verb) {
        auto* row = new QHBoxLayout();
        row->setSpacing(9);
        hint.face = new QLabel(footer);
        hint.face->setFixedSize(28, 28);
        hint.face->setAlignment(Qt::AlignCenter);
        hint.text = new QLabel(verb, footer);
        hint.text->setStyleSheet(QStringLiteral(
            "color: #f2efe8; font-size: 13px; background: transparent;"));
        row->addWidget(hint.face);
        row->addWidget(hint.text);
        hintsRow->addLayout(row);
    };
    buildHintCell(m_hintRebind,  "Rebind");
    buildHintCell(m_hintClear,   "Clear");
    buildHintCell(m_hintAutoMap, "Auto-Map");
    buildHintCell(m_hintClose,   "Close");

    footerLay->addLayout(hintsRow, 0);

    grid->addWidget(footer, kRowFooter, kColLeft, 1, 3);

    if (m_inputManager) {
        connect(m_inputManager, &SdlInputManager::controllerTypeChanged,
                this, &ControllerBindingsView::updateFaceHints);
    }
    updateFaceHints();

    reloadBindings();

    // Transient status banner (Auto-Map confirmations, etc.). Hidden
    // by default; positioned center-top of the body in showStatus().
    m_statusBanner = new QLabel(this);
    m_statusBanner->setAlignment(Qt::AlignCenter);
    m_statusBanner->setStyleSheet(QStringLiteral(
        "QLabel { background: #f59e0b; color: #1a1816;"
        "         font-size: 13px; font-weight: 700;"
        "         padding: 10px 24px; border-radius: 8px; }"));
    m_statusBanner->hide();

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        if (m_statusBanner) m_statusBanner->hide();
    });
}

ControllerBindingsView::~ControllerBindingsView() = default;

void ControllerBindingsView::setCapturing(bool capturing) {
    m_capturing = capturing;
    if (!capturing && m_capturingCard) {
        // reloadBindings() (called by the dialog right after) repaints
        // every card with its real value, but make absolutely sure the
        // capturing card's italic-amber state is cleared even if reload
        // is delayed.
        m_capturingCard->setCurrentValue(currentValueFor(m_capturingCard->def().key));
        m_capturingCard = nullptr;
    }
}

void ControllerBindingsView::buildSlots(const QVector<BindingDef>& bindings) {
    auto* grid = qobject_cast<QGridLayout*>(layout());
    Q_ASSERT(grid);

    struct SlotPos { int row; int col; };
    static const QHash<QString, SlotPos> slotPositions{
        {"DPad",            {kRowDpadFB,  kColLeft}},
        {"LeftAnalog",      {kRowAnalogs, kColLeft}},
        {"FaceButtons",     {kRowDpadFB,  kColRight}},
        {"RightAnalog",     {kRowAnalogs, kColRight}},
        {"LeftShoulders",   {kRowTop,     kColLeft}},
        {"RightShoulders",  {kRowTop,     kColRight}},
        {"Shoulders",       {kRowTop,     kColCenter}},   // legacy fallback
        {"System",          {kRowBottom,  kColCenter}},
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

        // Slots that lay their cards out as a horizontal strip with stacked
        // (label-top, value-below) cards: the four shoulder-column slots
        // and the system bottom strip. Side-column clusters are vertical
        // stacks of inline cards.
        const bool horizontal =
            (slot == "Shoulders" || slot == "System" ||
             slot == "LeftShoulders" || slot == "RightShoulders");
        const BindingCard::Style cardStyle = horizontal
            ? BindingCard::Style::Stacked
            : BindingCard::Style::Inline;

        auto* container = new QWidget(this);
        container->setStyleSheet("background: transparent;");
        if (!horizontal) container->setFixedWidth(kColumnWidth);
        auto* v = new QVBoxLayout(container);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(6);

        auto* header = new QLabel(sectionHeaderText(slot, m_slotTitleOverrides), container);
        header->setStyleSheet(QStringLiteral(
            "color: #f59e0b; font-size: 10px; font-weight: 600;"
            "letter-spacing: 1.8px; background: transparent;"));
        v->addWidget(header);

        QBoxLayout* cardLay = horizontal
            ? static_cast<QBoxLayout*>(new QHBoxLayout())
            : static_cast<QBoxLayout*>(new QVBoxLayout());
        cardLay->setSpacing(horizontal ? 8 : 6);
        cardLay->setContentsMargins(0, 0, 0, 0);

        for (const auto& b : bySlot[slot]) {
            // Skip bindings with no INI key (kept in the schema for save/restore
            // but have no card on the page — e.g. some legacy abstract entries).
            if (b.key.isEmpty()) continue;
            auto* card = new BindingCard(b, cardStyle, container);
            cardLay->addWidget(card);
            connect(card, &SettingsCard::focused, this, [this, card](const SettingDef&){
                onCardFocused(card->def());
            });
            connect(card, &SettingsCard::activated, this, [this, card](){
                if (m_capturingCard && m_capturingCard != card)
                    m_capturingCard->setCurrentValue(currentValueFor(m_capturingCard->def().key));
                m_capturingCard = card;
                card->setCapturingState(true);
                emit rebindRequested(card->def());
            });
            connect(card, &BindingCard::clearRequested, this,
                    &ControllerBindingsView::clearRequested);
        }
        if (horizontal) cardLay->addStretch(1);
        v->addLayout(cardLay);
        if (!horizontal) v->addStretch(1);

        if (slot == "LeftShoulders") {
            // Pin to the left of the shoulder row + add a stretch after,
            // so RightShoulders can pin to the right.
            m_shoulderRowLayout->addWidget(container);
            m_shoulderRowLayout->addStretch(1);
        } else if (slot == "RightShoulders") {
            m_shoulderRowLayout->addWidget(container);
        } else {
            grid->addWidget(container, pos.row, pos.col);
        }
    }
}

void ControllerBindingsView::reloadBindings() {
    // nullptr only in widget tests — production callers always pass a real AppController.
    if (m_appController) {
        const QVariantList raw = m_appController->controllerBindingsForPort(
            m_emuId, m_port, m_controllerTypeId);
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

void ControllerBindingsView::updateFaceHints() {
    if (!m_inputManager) return;
    const int t = m_inputManager->controllerType();   // 0 Keyboard, 1 Xbox, 2 PlayStation

    auto applyGlyph = [](QLabel* face, const QString& glyph,
                          const QString& bg, const QString& fg, int fontSize) {
        face->setText(glyph);
        face->setStyleSheet(QString(
            "background: %1; color: %2; border-radius: 14px;"
            "font-size: %3px; font-weight: 800;").arg(bg, fg).arg(fontSize));
    };

    if (t == 2) {  // PlayStation — face glyphs at the SAME physical positions:
        // Rebind  = bottom (Cross),    Clear = right (Circle),
        // Auto-Map = top (Triangle),   Close = left (Square).
        applyGlyph(m_hintRebind.face,  "✕", "#3aa6ff", "#ffffff", 16);
        applyGlyph(m_hintClear.face,   "○", "#e74c4c", "#ffffff", 16);
        applyGlyph(m_hintAutoMap.face, "△", "#39c46b", "#ffffff", 14);
        applyGlyph(m_hintClose.face,   "□", "#cc7adb", "#ffffff", 14);
    } else if (t == 1) {  // Xbox — A, B, Y, X
        applyGlyph(m_hintRebind.face,  "A", "#39c46b", "#0e2a14", 13);
        applyGlyph(m_hintClear.face,   "B", "#e74c4c", "#2a0e0e", 13);
        applyGlyph(m_hintAutoMap.face, "Y", "#ffd23a", "#2a210e", 13);
        applyGlyph(m_hintClose.face,   "X", "#3aa6ff", "#0e1f2a", 13);
    } else {  // Keyboard — show key letters in a neutral grey.
        applyGlyph(m_hintRebind.face,  "↵",  "#706c66", "#f2efe8", 14);
        applyGlyph(m_hintClear.face,   "⌫",  "#706c66", "#f2efe8", 14);
        applyGlyph(m_hintAutoMap.face, "M",       "#706c66", "#f2efe8", 13);
        applyGlyph(m_hintClose.face,   "Esc",     "#706c66", "#f2efe8", 10);
    }
}

void ControllerBindingsView::showStatus(const QString& message) {
    if (!m_statusBanner) return;
    m_statusBanner->setText(message);
    m_statusBanner->adjustSize();
    // Centered horizontally, ~70px from the top of the view.
    int x = qMax(0, (width() - m_statusBanner->width()) / 2);
    m_statusBanner->move(x, 70);
    m_statusBanner->show();
    m_statusBanner->raise();
    if (m_statusTimer) m_statusTimer->start(2500);
}

void ControllerBindingsView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!m_imageArea) return;
    // Image scales with the page. Driven by the loaded SVG's intrinsic
    // aspect ratio so wide controllers (PSP, ~2.33:1) don't end up
    // letterboxed inside a DualShock-shaped (~1.46:1) box.
    //
    // We start by picking ~50% of the view width and deriving the height,
    // then clamp the height to a similar fraction of the view height. If
    // the height clamp kicks in, recompute the width from the aspect so
    // the box stays proportional. Recomputed on every resize so dragging
    // the dialog grows the controller proportionally.
    const qreal aspect = m_imageArea->aspectRatio();
    int maxW = qBound(kImageMinW, width() / 2, 1100);
    int maxH = static_cast<int>(maxW / aspect);
    const int hCap = qMax(kImageMinH, height() * 6 / 10);
    if (maxH > hCap) {
        maxH = hCap;
        maxW = static_cast<int>(maxH * aspect);
    }
    m_imageArea->setMaximumSize(maxW, maxH);
}

// Inner classes defined in this .cpp file carry Q_OBJECT, so their moc
// output must be included here (AUTOMOC only processes headers).
#include "controller_bindings_view.moc"
