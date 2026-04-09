#include "ds_negcon_bindings_widget.h"
#include "binding_widget_common.h"

#include <QPixmap>

static constexpr int kTy   = 16;

#define LBL(name) findChild<QLabel*>(name)

// ─────────────────────────────────────────────────────────────

DSNegconBindingsWidget::DSNegconBindingsWidget(SdlInputManager* inputManager,
                                               AppController* appController,
                                               const QString& emuId,
                                               int port,
                                               QWidget* parent)
    : BindingsWidgetBase(inputManager, appController, emuId, port, parent)
{
    auto* canvas = this;

    // -- LEFT: D-Pad --
    auto* dpadTitle = makeLabel(canvas, "D-Pad", 15, true);
    m_dpadBox = makeBox(canvas);
    auto* dpadUp_l = makeLabel(canvas, "Up");
    auto* dpadUp   = new BindBtn(canvas);
    auto* dpadL_l  = makeLabel(canvas, "Left");
    auto* dpadR_l  = makeLabel(canvas, "Right");
    auto* dpadLeft = new BindBtn(canvas);
    auto* dpadRight= new BindBtn(canvas);
    auto* dpadDn_l = makeLabel(canvas, "Down");
    auto* dpadDown = new BindBtn(canvas);

    // -- RIGHT: Face Buttons (A, B, I, II) --
    auto* fbTitle  = makeLabel(canvas, "Face Buttons", 15, true);
    m_faceBox      = makeBox(canvas);
    auto* aLbl     = makeLabel(canvas, "A");
    auto* aBtn     = new BindBtn(canvas);
    auto* bLbl     = makeLabel(canvas, "B");
    auto* iLbl     = makeLabel(canvas, "I");
    auto* bBtn     = new BindBtn(canvas);
    auto* iBtn     = new BindBtn(canvas);
    auto* iiLbl    = makeLabel(canvas, "II");
    auto* iiBtn    = new BindBtn(canvas);

    // -- CENTER --
    auto* lLbl     = makeLabel(canvas, "L");
    auto* lBtn     = new BindBtn(canvas);
    auto* rLbl     = makeLabel(canvas, "R");
    auto* rBtn     = new BindBtn(canvas);
    auto* startLbl = makeLabel(canvas, "Start");
    auto* start    = new BindBtn(canvas);

    // Controller image
    m_imgLabel = new QLabel(canvas);
    QPixmap pix(":/AppUI/qml/AppUI/images/controllers/ds_negcon.svg");
    if (!pix.isNull())
        m_imgLabel->setPixmap(pix.scaled(420, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setStyleSheet("background: transparent; border: none;");

    // Bottom center: Steering
    auto* steerLLbl = makeLabel(canvas, "Steering Left");
    auto* steerL    = new BindBtn(canvas);
    auto* steerRLbl = makeLabel(canvas, "Steering Right");
    auto* steerR    = new BindBtn(canvas);

    // Style all binding buttons
    setupBtn(dpadUp, "Up"); setupBtn(dpadDown, "Down");
    setupBtn(dpadLeft, "Left"); setupBtn(dpadRight, "Right");
    setupBtn(aBtn, "A"); setupBtn(bBtn, "B");
    setupBtn(iBtn, "I"); setupBtn(iiBtn, "II");
    setupBtn(lBtn, "L"); setupBtn(rBtn, "R");
    setupBtn(start, "Start");
    setupBtn(steerL, "Steering Left"); setupBtn(steerR, "Steering Right");

    // Set object names on labels for lookup in relayout()
    dpadTitle->setObjectName("dpadTitle");
    dpadUp_l->setObjectName("dpadUp_l"); dpadL_l->setObjectName("dpadL_l");
    dpadR_l->setObjectName("dpadR_l"); dpadDn_l->setObjectName("dpadDn_l");
    fbTitle->setObjectName("fbTitle");
    aLbl->setObjectName("aLbl"); bLbl->setObjectName("bLbl");
    iLbl->setObjectName("iLbl"); iiLbl->setObjectName("iiLbl");
    lLbl->setObjectName("lLbl"); rLbl->setObjectName("rLbl");
    startLbl->setObjectName("startLbl");
    steerLLbl->setObjectName("steerLLbl"); steerRLbl->setObjectName("steerRLbl");

    loadBindings();
    relayout();
}

void DSNegconBindingsWidget::relayout() {
    int W = width();
    int H = height();
    Q_UNUSED(H);

    int margin = 16;
    int colW = qMax(280, static_cast<int>(W * 0.24));
    int btnW = qMin(160, colW - 40);
    int btnSmW = qMin(130, (colW - 30) / 2);
    int boxH = 230;

    int lCol = margin;
    int rColX = W - margin - colW;
    int cx = W / 2;
    int cLeft = lCol + colW + 10;
    int cRight = rColX - 10;

    // ── LEFT: D-Pad ──
    int lMid = lCol + colW / 2;
    if (auto* t = LBL("dpadTitle")) t->move(lCol, kTy + 16);
    m_dpadBox->setGeometry(lCol, kTy + 38, colW, boxH);

    if (auto* l = LBL("dpadUp_l")) l->move(lMid - 10, kTy + 56);
    m_bindingButtons["Up"]->setGeometry(lMid - btnW/2, kTy + 74, btnW, kBtnH);

    if (auto* l = LBL("dpadL_l")) l->move(lCol + 12, kTy + 120);
    if (auto* l = LBL("dpadR_l")) l->move(lCol + colW - 40, kTy + 120);
    m_bindingButtons["Left"]->setGeometry(lCol + 10, kTy + 138, btnSmW, kBtnH);
    m_bindingButtons["Right"]->setGeometry(lCol + colW - btnSmW - 10, kTy + 138, btnSmW, kBtnH);

    if (auto* l = LBL("dpadDn_l")) l->move(lMid - 15, kTy + 184);
    m_bindingButtons["Down"]->setGeometry(lMid - btnW/2, kTy + 202, btnW, kBtnH);

    // ── RIGHT: Face Buttons (A top, B left, I right, II bottom) ──
    int rMid = rColX + colW / 2;
    if (auto* t = LBL("fbTitle")) t->move(rColX + 6, kTy + 16);
    m_faceBox->setGeometry(rColX, kTy + 38, colW, boxH);

    if (auto* l = LBL("aLbl")) l->move(rMid - 5, kTy + 56);
    m_bindingButtons["A"]->setGeometry(rMid - btnW/2, kTy + 74, btnW, kBtnH);

    if (auto* l = LBL("bLbl")) l->move(rColX + 14, kTy + 120);
    if (auto* l = LBL("iLbl")) l->move(rColX + colW - 20, kTy + 120);
    m_bindingButtons["B"]->setGeometry(rColX + 10, kTy + 138, btnSmW, kBtnH);
    m_bindingButtons["I"]->setGeometry(rColX + colW - btnSmW - 10, kTy + 138, btnSmW, kBtnH);

    if (auto* l = LBL("iiLbl")) l->move(rMid - 5, kTy + 184);
    m_bindingButtons["II"]->setGeometry(rMid - btnW/2, kTy + 202, btnW, kBtnH);

    // ── CENTER: L/R ──
    if (auto* l = LBL("lLbl")) l->move(cLeft + 10, kTy + 16);
    m_bindingButtons["L"]->setGeometry(cLeft, kTy + 36, btnW, kBtnH);

    if (auto* l = LBL("rLbl")) l->move(cRight - btnW + 10, kTy + 16);
    m_bindingButtons["R"]->setGeometry(cRight - btnW, kTy + 36, btnW, kBtnH);

    // ── CENTER: Start ──
    if (auto* l = LBL("startLbl")) l->move(cx - 18, kTy + 100);
    m_bindingButtons["Start"]->setGeometry(cx - btnSmW/2, kTy + 120, btnSmW, kBtnH);

    // ── CENTER: Controller image ──
    int imgW = qMin(420, cRight - cLeft);
    int imgH = static_cast<int>(imgW * 0.71);
    m_imgLabel->setGeometry(cx - imgW/2, kTy + 180, imgW, imgH);

    // ── CENTER: Steering Left, Steering Right ──
    int row3Y = kTy + 180 + imgH + 10;
    int itemW = 170;
    int spacing = 10;

    if (auto* l = LBL("steerLLbl")) l->move(cx - itemW - spacing/2 + itemW/2 - 38, row3Y);
    m_bindingButtons["Steering Left"]->setGeometry(cx - itemW - spacing/2, row3Y + 18, itemW, kBtnH);

    if (auto* l = LBL("steerRLbl")) l->move(cx + spacing/2 + itemW/2 - 42, row3Y);
    m_bindingButtons["Steering Right"]->setGeometry(cx + spacing/2, row3Y + 18, itemW, kBtnH);
}
