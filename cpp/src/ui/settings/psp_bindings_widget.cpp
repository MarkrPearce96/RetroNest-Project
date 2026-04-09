#include "psp_bindings_widget.h"
#include "binding_widget_common.h"

static constexpr int kTy = 16;

#define LBL(name) findChild<QLabel*>(name)

PSPBindingsWidget::PSPBindingsWidget(SdlInputManager* inputManager,
                                     AppController* appController,
                                     const QString& emuId,
                                     int port,
                                     QWidget* parent)
    : BindingsWidgetBase(inputManager, appController, emuId, port, parent)
{
    auto* canvas = this;

    // ── LEFT: D-Pad ──
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

    // ── RIGHT: Face Buttons ──
    auto* fbTitle  = makeLabel(canvas, "Face Buttons", 15, true);
    m_faceBox      = makeBox(canvas);
    auto* triLbl   = makeLabel(canvas, "Triangle");
    auto* tri      = new BindBtn(canvas);
    auto* sqLbl    = makeLabel(canvas, "Square");
    auto* cirLbl   = makeLabel(canvas, "Circle");
    auto* sq       = new BindBtn(canvas);
    auto* cir      = new BindBtn(canvas);
    auto* crossLbl = makeLabel(canvas, "Cross");
    auto* cross    = new BindBtn(canvas);

    // ── CENTER: L/R, Select, Start ──
    auto* lLbl     = makeLabel(canvas, "L");
    auto* l        = new BindBtn(canvas);
    auto* rLbl     = makeLabel(canvas, "R");
    auto* r        = new BindBtn(canvas);
    auto* selLbl   = makeLabel(canvas, "Select");
    auto* sel      = new BindBtn(canvas);
    auto* startLbl = makeLabel(canvas, "Start");
    auto* start    = new BindBtn(canvas);

    // ── BOTTOM-LEFT: Analog Stick ──
    auto* analogTitle = makeLabel(canvas, "Analog Stick", 15, true);
    m_analogBox    = makeBox(canvas);
    auto* anUpLbl  = makeLabel(canvas, "Up");
    auto* anUp     = new BindBtn(canvas);
    auto* anDnLbl  = makeLabel(canvas, "Down");
    auto* anDn     = new BindBtn(canvas);
    auto* anLLbl   = makeLabel(canvas, "Left");
    auto* anL      = new BindBtn(canvas);
    auto* anRLbl   = makeLabel(canvas, "Right");
    auto* anR      = new BindBtn(canvas);

    // Setup binding buttons with labels matching controllerBindingDefs() labels
    setupBtn(dpadUp, "Up"); setupBtn(dpadDown, "Down");
    setupBtn(dpadLeft, "Left"); setupBtn(dpadRight, "Right");
    setupBtn(tri, "Triangle"); setupBtn(cross, "Cross");
    setupBtn(sq, "Square"); setupBtn(cir, "Circle");
    setupBtn(l, "L"); setupBtn(r, "R");
    setupBtn(sel, "Select"); setupBtn(start, "Start");
    setupBtn(anUp, "An.Up"); setupBtn(anDn, "An.Down");
    setupBtn(anL, "An.Left"); setupBtn(anR, "An.Right");

    // Set object names for lookup in relayout()
    dpadTitle->setObjectName("dpadTitle");
    dpadUp_l->setObjectName("dpadUp_l"); dpadL_l->setObjectName("dpadL_l");
    dpadR_l->setObjectName("dpadR_l"); dpadDn_l->setObjectName("dpadDn_l");
    fbTitle->setObjectName("fbTitle");
    triLbl->setObjectName("triLbl"); sqLbl->setObjectName("sqLbl");
    cirLbl->setObjectName("cirLbl"); crossLbl->setObjectName("crossLbl");
    lLbl->setObjectName("lLbl"); rLbl->setObjectName("rLbl");
    selLbl->setObjectName("selLbl"); startLbl->setObjectName("startLbl");
    analogTitle->setObjectName("analogTitle");
    anUpLbl->setObjectName("anUpLbl"); anDnLbl->setObjectName("anDnLbl");
    anLLbl->setObjectName("anLLbl"); anRLbl->setObjectName("anRLbl");

    loadBindings();
    relayout();
}

void PSPBindingsWidget::relayout() {
    int W = width();
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

    if (auto* lb = LBL("dpadUp_l")) lb->move(lMid - 10, kTy + 56);
    m_bindingButtons["Up"]->setGeometry(lMid - btnW/2, kTy + 74, btnW, kBtnH);

    if (auto* lb = LBL("dpadL_l")) lb->move(lCol + 12, kTy + 120);
    if (auto* lb = LBL("dpadR_l")) lb->move(lCol + colW - 40, kTy + 120);
    m_bindingButtons["Left"]->setGeometry(lCol + 10, kTy + 138, btnSmW, kBtnH);
    m_bindingButtons["Right"]->setGeometry(lCol + colW - btnSmW - 10, kTy + 138, btnSmW, kBtnH);

    if (auto* lb = LBL("dpadDn_l")) lb->move(lMid - 15, kTy + 184);
    m_bindingButtons["Down"]->setGeometry(lMid - btnW/2, kTy + 202, btnW, kBtnH);

    // ── RIGHT: Face Buttons ──
    int rMid = rColX + colW / 2;
    if (auto* t = LBL("fbTitle")) t->move(rColX + 6, kTy + 16);
    m_faceBox->setGeometry(rColX, kTy + 38, colW, boxH);

    if (auto* lb = LBL("triLbl")) lb->move(rMid - 25, kTy + 56);
    m_bindingButtons["Triangle"]->setGeometry(rMid - btnW/2, kTy + 74, btnW, kBtnH);

    if (auto* lb = LBL("sqLbl")) lb->move(rColX + 14, kTy + 120);
    if (auto* lb = LBL("cirLbl")) lb->move(rColX + colW - 46, kTy + 120);
    m_bindingButtons["Square"]->setGeometry(rColX + 10, kTy + 138, btnSmW, kBtnH);
    m_bindingButtons["Circle"]->setGeometry(rColX + colW - btnSmW - 10, kTy + 138, btnSmW, kBtnH);

    if (auto* lb = LBL("crossLbl")) lb->move(rMid - 18, kTy + 184);
    m_bindingButtons["Cross"]->setGeometry(rMid - btnW/2, kTy + 202, btnW, kBtnH);

    // ── CENTER: L/R ──
    if (auto* lb = LBL("lLbl")) lb->move(cLeft + 10, kTy + 16);
    m_bindingButtons["L"]->setGeometry(cLeft, kTy + 36, btnW, kBtnH);

    if (auto* lb = LBL("rLbl")) lb->move(cRight - btnW + 10, kTy + 16);
    m_bindingButtons["R"]->setGeometry(cRight - btnW, kTy + 36, btnW, kBtnH);

    // ── CENTER: Select, Start ──
    if (auto* lb = LBL("selLbl")) lb->move(cx - 70, kTy + 100);
    m_bindingButtons["Select"]->setGeometry(cx - btnSmW - 5, kTy + 120, btnSmW, kBtnH);

    if (auto* lb = LBL("startLbl")) lb->move(cx + 30, kTy + 100);
    m_bindingButtons["Start"]->setGeometry(cx + 5, kTy + 120, btnSmW, kBtnH);

    // ── BOTTOM-LEFT: Analog Stick ──
    if (auto* t = LBL("analogTitle")) t->move(lCol, kTy + 290);
    m_analogBox->setGeometry(lCol, kTy + 312, colW, boxH);

    if (auto* lb = LBL("anUpLbl")) lb->move(lMid - 10, kTy + 330);
    m_bindingButtons["An.Up"]->setGeometry(lMid - btnW/2, kTy + 348, btnW, kBtnH);

    if (auto* lb = LBL("anLLbl")) lb->move(lCol + 12, kTy + 394);
    if (auto* lb = LBL("anRLbl")) lb->move(lCol + colW - 40, kTy + 394);
    m_bindingButtons["An.Left"]->setGeometry(lCol + 10, kTy + 412, btnSmW, kBtnH);
    m_bindingButtons["An.Right"]->setGeometry(lCol + colW - btnSmW - 10, kTy + 412, btnSmW, kBtnH);

    if (auto* lb = LBL("anDnLbl")) lb->move(lMid - 15, kTy + 458);
    m_bindingButtons["An.Down"]->setGeometry(lMid - btnW/2, kTy + 476, btnW, kBtnH);
}
