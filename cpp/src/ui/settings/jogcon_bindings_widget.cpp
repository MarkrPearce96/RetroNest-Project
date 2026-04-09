#include "jogcon_bindings_widget.h"
#include "binding_widget_common.h"

#include <QPixmap>

static constexpr int kTy   = 16;

#define LBL(name) findChild<QLabel*>(name)

// ─────────────────────────────────────────────────────────────

JogconBindingsWidget::JogconBindingsWidget(SdlInputManager* inputManager,
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

    // -- RIGHT: Face Buttons --
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

    // -- CENTER --
    auto* l2Lbl    = makeLabel(canvas, "L2");
    auto* l2       = new BindBtn(canvas);
    auto* r2Lbl    = makeLabel(canvas, "R2");
    auto* r2       = new BindBtn(canvas);
    auto* l1Lbl    = makeLabel(canvas, "L1");
    auto* l1       = new BindBtn(canvas);
    auto* selLbl   = makeLabel(canvas, "Select");
    auto* sel      = new BindBtn(canvas);
    auto* startLbl = makeLabel(canvas, "Start");
    auto* start    = new BindBtn(canvas);
    auto* r1Lbl    = makeLabel(canvas, "R1");
    auto* r1       = new BindBtn(canvas);

    // Controller image
    m_imgLabel = new QLabel(canvas);
    QPixmap pix(":/AppUI/qml/AppUI/images/controllers/Jogcon.svg");
    if (!pix.isNull())
        m_imgLabel->setPixmap(pix.scaled(420, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setStyleSheet("background: transparent; border: none;");

    // Bottom center: Dial Left, Dial Right
    auto* dlLbl    = makeLabel(canvas, "Dial Left");
    auto* dl       = new BindBtn(canvas);
    auto* drLbl    = makeLabel(canvas, "Dial Right");
    auto* dr       = new BindBtn(canvas);

    // Motors
    auto* lgMotorLbl = makeLabel(canvas, "Large Motor");
    auto* lgMotor    = new BindBtn(canvas);
    auto* smMotorLbl = makeLabel(canvas, "Small Motor");
    auto* smMotor    = new BindBtn(canvas);

    // Style all binding buttons
    setupBtn(dpadUp, "Up"); setupBtn(dpadDown, "Down");
    setupBtn(dpadLeft, "Left"); setupBtn(dpadRight, "Right");
    setupBtn(tri, "Triangle"); setupBtn(cross, "Cross");
    setupBtn(sq, "Square"); setupBtn(cir, "Circle");
    setupBtn(l2, "L2"); setupBtn(r2, "R2");
    setupBtn(l1, "L1"); setupBtn(r1, "R1");
    setupBtn(sel, "Select"); setupBtn(start, "Start");
    setupBtn(dl, "Dial Left"); setupBtn(dr, "Dial Right");
    setupBtn(lgMotor, "LargeMotor"); setupBtn(smMotor, "SmallMotor");

    // Set object names on labels for lookup in relayout()
    dpadTitle->setObjectName("dpadTitle");
    dpadUp_l->setObjectName("dpadUp_l"); dpadL_l->setObjectName("dpadL_l");
    dpadR_l->setObjectName("dpadR_l"); dpadDn_l->setObjectName("dpadDn_l");
    fbTitle->setObjectName("fbTitle");
    triLbl->setObjectName("triLbl"); sqLbl->setObjectName("sqLbl");
    cirLbl->setObjectName("cirLbl"); crossLbl->setObjectName("crossLbl");
    l2Lbl->setObjectName("l2Lbl"); r2Lbl->setObjectName("r2Lbl");
    l1Lbl->setObjectName("l1Lbl"); r1Lbl->setObjectName("r1Lbl");
    selLbl->setObjectName("selLbl"); startLbl->setObjectName("startLbl");
    dlLbl->setObjectName("dlLbl"); drLbl->setObjectName("drLbl");
    lgMotorLbl->setObjectName("lgMotorLbl"); smMotorLbl->setObjectName("smMotorLbl");

    loadBindings();
    relayout();
}

void JogconBindingsWidget::relayout() {
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

    // ── RIGHT: Face Buttons ──
    int rMid = rColX + colW / 2;
    if (auto* t = LBL("fbTitle")) t->move(rColX + 6, kTy + 16);
    m_faceBox->setGeometry(rColX, kTy + 38, colW, boxH);

    if (auto* l = LBL("triLbl")) l->move(rMid - 25, kTy + 56);
    m_bindingButtons["Triangle"]->setGeometry(rMid - btnW/2, kTy + 74, btnW, kBtnH);

    if (auto* l = LBL("sqLbl")) l->move(rColX + 14, kTy + 120);
    if (auto* l = LBL("cirLbl")) l->move(rColX + colW - 46, kTy + 120);
    m_bindingButtons["Square"]->setGeometry(rColX + 10, kTy + 138, btnSmW, kBtnH);
    m_bindingButtons["Circle"]->setGeometry(rColX + colW - btnSmW - 10, kTy + 138, btnSmW, kBtnH);

    if (auto* l = LBL("crossLbl")) l->move(rMid - 18, kTy + 184);
    m_bindingButtons["Cross"]->setGeometry(rMid - btnW/2, kTy + 202, btnW, kBtnH);

    // ── CENTER: L2/R2 ──
    if (auto* l = LBL("l2Lbl")) l->move(cLeft + 10, kTy + 16);
    m_bindingButtons["L2"]->setGeometry(cLeft, kTy + 36, btnW, kBtnH);

    if (auto* l = LBL("r2Lbl")) l->move(cRight - btnW + 10, kTy + 16);
    m_bindingButtons["R2"]->setGeometry(cRight - btnW, kTy + 36, btnW, kBtnH);

    // ── CENTER: L1, Select, Start, R1 ──
    if (auto* l = LBL("l1Lbl")) l->move(cLeft + 10, kTy + 100);
    m_bindingButtons["L1"]->setGeometry(cLeft, kTy + 120, btnSmW, kBtnH);

    if (auto* l = LBL("selLbl")) l->move(cx - 70, kTy + 100);
    m_bindingButtons["Select"]->setGeometry(cx - btnSmW - 5, kTy + 120, btnSmW, kBtnH);

    if (auto* l = LBL("startLbl")) l->move(cx + 30, kTy + 100);
    m_bindingButtons["Start"]->setGeometry(cx + 5, kTy + 120, btnSmW, kBtnH);

    if (auto* l = LBL("r1Lbl")) l->move(cRight - btnSmW + 10, kTy + 100);
    m_bindingButtons["R1"]->setGeometry(cRight - btnSmW, kTy + 120, btnSmW, kBtnH);

    // ── CENTER: Controller image ──
    int imgW = qMin(420, cRight - cLeft);
    int imgH = static_cast<int>(imgW * 0.71);
    m_imgLabel->setGeometry(cx - imgW/2, kTy + 180, imgW, imgH);

    // ── CENTER: Dial Left, Dial Right ──
    int dialY = kTy + 180 + imgH + 10;
    int dialW = 170;
    int dialSpacing = 20;

    if (auto* l = LBL("dlLbl")) l->move(cx - dialW - dialSpacing/2 + dialW/2 - 25, dialY);
    m_bindingButtons["Dial Left"]->setGeometry(cx - dialW - dialSpacing/2, dialY + 18, dialW, kBtnH);

    if (auto* l = LBL("drLbl")) l->move(cx + dialSpacing/2 + dialW/2 - 30, dialY);
    m_bindingButtons["Dial Right"]->setGeometry(cx + dialSpacing/2, dialY + 18, dialW, kBtnH);

    // ── CENTER: LargeMotor, SmallMotor ──
    int motorY = dialY + 60;

    if (auto* l = LBL("lgMotorLbl")) l->move(cx - dialW - dialSpacing/2 + dialW/2 - 35, motorY);
    m_bindingButtons["LargeMotor"]->setGeometry(cx - dialW - dialSpacing/2, motorY + 18, dialW, kBtnH);

    if (auto* l = LBL("smMotorLbl")) l->move(cx + dialSpacing/2 + dialW/2 - 35, motorY);
    m_bindingButtons["SmallMotor"]->setGeometry(cx + dialSpacing/2, motorY + 18, dialW, kBtnH);
}
