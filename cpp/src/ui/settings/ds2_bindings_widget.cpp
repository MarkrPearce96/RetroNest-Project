#include "ds2_bindings_widget.h"
#include "binding_widget_common.h"

#include <QPixmap>

static constexpr int kTy   = 16;   // no toolbar, so reduced top offset

// ── Find child label by name ────────────────────────────────
#define LBL(name) findChild<QLabel*>(name)

// ─────────────────────────────────────────────────────────────

DS2BindingsWidget::DS2BindingsWidget(SdlInputManager* inputManager,
                                     AppController* appController,
                                     const QString& emuId,
                                     int port,
                                     QWidget* parent)
    : BindingsWidgetBase(inputManager, appController, emuId, port, parent)
{
    auto* canvas = this;

    // ============ Create all widgets (positions set in relayout) ============

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

    // -- LEFT: Left Analog --
    auto* laTitle  = makeLabel(canvas, "Left Analog", 15, true);
    m_lAnalogBox   = makeBox(canvas);
    auto* laUp_l   = makeLabel(canvas, "Up");
    auto* laUp     = new BindBtn(canvas);
    auto* laL_l    = makeLabel(canvas, "Left");
    auto* laR_l    = makeLabel(canvas, "Right");
    auto* laLeft   = new BindBtn(canvas);
    auto* laRight  = new BindBtn(canvas);
    auto* laDn_l   = makeLabel(canvas, "Down");
    auto* laDown   = new BindBtn(canvas);

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

    // -- RIGHT: Right Analog --
    auto* raTitle  = makeLabel(canvas, "Right Analog", 15, true);
    m_rAnalogBox   = makeBox(canvas);
    auto* raUp_l   = makeLabel(canvas, "Up");
    auto* raUp     = new BindBtn(canvas);
    auto* raL_l    = makeLabel(canvas, "Left");
    auto* raR_l    = makeLabel(canvas, "Right");
    auto* raLeft   = new BindBtn(canvas);
    auto* raRight  = new BindBtn(canvas);
    auto* raDn_l   = makeLabel(canvas, "Down");
    auto* raDown   = new BindBtn(canvas);

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
    QPixmap pix(":/AppUI/qml/AppUI/images/controllers/DualShock_2.svg");
    if (!pix.isNull())
        m_imgLabel->setPixmap(pix.scaled(420, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setStyleSheet("background: transparent; border: none;");

    // Bottom center buttons
    auto* l3Lbl    = makeLabel(canvas, "L3");
    auto* l3       = new BindBtn(canvas);
    auto* pmLbl    = makeLabel(canvas, "Pressure Modifier");
    auto* pm       = new BindBtn(canvas);
    auto* r3Lbl    = makeLabel(canvas, "R3");
    auto* r3       = new BindBtn(canvas);
    auto* anaLbl   = makeLabel(canvas, "Analog");
    auto* ana      = new BindBtn(canvas);

    // Style all binding buttons
    setupBtn(dpadUp, "Up"); setupBtn(dpadDown, "Down");
    setupBtn(dpadLeft, "Left"); setupBtn(dpadRight, "Right");
    setupBtn(laUp, "Left Stick Up"); setupBtn(laDown, "Left Stick Down");
    setupBtn(laLeft, "Left Stick Left"); setupBtn(laRight, "Left Stick Right");
    setupBtn(tri, "Triangle"); setupBtn(cross, "Cross");
    setupBtn(sq, "Square"); setupBtn(cir, "Circle");
    setupBtn(raUp, "Right Stick Up"); setupBtn(raDown, "Right Stick Down");
    setupBtn(raLeft, "Right Stick Left"); setupBtn(raRight, "Right Stick Right");
    setupBtn(l2, "L2"); setupBtn(r2, "R2");
    setupBtn(l1, "L1"); setupBtn(r1, "R1");
    setupBtn(sel, "Select"); setupBtn(start, "Start");
    setupBtn(l3, "L3"); setupBtn(r3, "R3");
    setupBtn(pm, "Pressure Modifier"); setupBtn(ana, "Analog");

    // Set object names on labels for lookup in relayout()
    dpadTitle->setObjectName("dpadTitle");
    dpadUp_l->setObjectName("dpadUp_l"); dpadL_l->setObjectName("dpadL_l");
    dpadR_l->setObjectName("dpadR_l"); dpadDn_l->setObjectName("dpadDn_l");
    laTitle->setObjectName("laTitle");
    laUp_l->setObjectName("laUp_l"); laL_l->setObjectName("laL_l");
    laR_l->setObjectName("laR_l"); laDn_l->setObjectName("laDn_l");
    fbTitle->setObjectName("fbTitle");
    triLbl->setObjectName("triLbl"); sqLbl->setObjectName("sqLbl");
    cirLbl->setObjectName("cirLbl"); crossLbl->setObjectName("crossLbl");
    raTitle->setObjectName("raTitle");
    raUp_l->setObjectName("raUp_l"); raL_l->setObjectName("raL_l");
    raR_l->setObjectName("raR_l"); raDn_l->setObjectName("raDn_l");
    l2Lbl->setObjectName("l2Lbl"); r2Lbl->setObjectName("r2Lbl");
    l1Lbl->setObjectName("l1Lbl"); r1Lbl->setObjectName("r1Lbl");
    selLbl->setObjectName("selLbl"); startLbl->setObjectName("startLbl");
    l3Lbl->setObjectName("l3Lbl"); pmLbl->setObjectName("pmLbl");
    r3Lbl->setObjectName("r3Lbl"); anaLbl->setObjectName("anaLbl");

    loadBindings();
    relayout();
}

void DS2BindingsWidget::relayout() {
    int W = width();
    int H = height();
    Q_UNUSED(H);

    int margin = 16;
    int colW = qMax(280, static_cast<int>(W * 0.24));
    int btnW = qMin(160, colW - 40);
    int btnSmW = qMin(130, (colW - 30) / 2);
    int boxH = 230;

    int lCol = margin;                  // left column x
    int rColX = W - margin - colW;      // right column x
    int cx = W / 2;                     // center x
    int cLeft = lCol + colW + 10;       // center-left edge
    int cRight = rColX - 10;            // center-right edge

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

    // ── LEFT: Left Analog ──
    if (auto* t = LBL("laTitle")) t->move(lCol, kTy + 290);
    m_lAnalogBox->setGeometry(lCol, kTy + 312, colW, boxH);

    if (auto* l = LBL("laUp_l")) l->move(lMid - 10, kTy + 330);
    m_bindingButtons["Left Stick Up"]->setGeometry(lMid - btnW/2, kTy + 348, btnW, kBtnH);

    if (auto* l = LBL("laL_l")) l->move(lCol + 12, kTy + 394);
    if (auto* l = LBL("laR_l")) l->move(lCol + colW - 40, kTy + 394);
    m_bindingButtons["Left Stick Left"]->setGeometry(lCol + 10, kTy + 412, btnSmW, kBtnH);
    m_bindingButtons["Left Stick Right"]->setGeometry(lCol + colW - btnSmW - 10, kTy + 412, btnSmW, kBtnH);

    if (auto* l = LBL("laDn_l")) l->move(lMid - 15, kTy + 458);
    m_bindingButtons["Left Stick Down"]->setGeometry(lMid - btnW/2, kTy + 476, btnW, kBtnH);

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

    // ── RIGHT: Right Analog ──
    if (auto* t = LBL("raTitle")) t->move(rColX + 6, kTy + 290);
    m_rAnalogBox->setGeometry(rColX, kTy + 312, colW, boxH);

    if (auto* l = LBL("raUp_l")) l->move(rMid - 10, kTy + 330);
    m_bindingButtons["Right Stick Up"]->setGeometry(rMid - btnW/2, kTy + 348, btnW, kBtnH);

    if (auto* l = LBL("raL_l")) l->move(rColX + 14, kTy + 394);
    if (auto* l = LBL("raR_l")) l->move(rColX + colW - 40, kTy + 394);
    m_bindingButtons["Right Stick Left"]->setGeometry(rColX + 10, kTy + 412, btnSmW, kBtnH);
    m_bindingButtons["Right Stick Right"]->setGeometry(rColX + colW - btnSmW - 10, kTy + 412, btnSmW, kBtnH);

    if (auto* l = LBL("raDn_l")) l->move(rMid - 15, kTy + 458);
    m_bindingButtons["Right Stick Down"]->setGeometry(rMid - btnW/2, kTy + 476, btnW, kBtnH);

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

    // ── CENTER: L3, Pressure Modifier, R3 ──
    int row3W = 170;
    int row3Y = kTy + 180 + imgH + 10;
    int row3Spacing = 10;
    int row3TotalW = row3W * 3 + row3Spacing * 2;
    int row3Start = cx - row3TotalW / 2;

    if (auto* l = LBL("l3Lbl")) l->move(row3Start + row3W/2 - 8, row3Y);
    m_bindingButtons["L3"]->setGeometry(row3Start, row3Y + 18, row3W, kBtnH);

    if (auto* l = LBL("pmLbl")) l->move(cx - 50, row3Y);
    m_bindingButtons["Pressure Modifier"]->setGeometry(row3Start + row3W + row3Spacing, row3Y + 18, row3W, kBtnH);

    if (auto* l = LBL("r3Lbl")) l->move(row3Start + 2*(row3W + row3Spacing) + row3W/2 - 8, row3Y);
    m_bindingButtons["R3"]->setGeometry(row3Start + 2*(row3W + row3Spacing), row3Y + 18, row3W, kBtnH);

    // ── CENTER: Analog ──
    int anaY = row3Y + 70;
    if (auto* l = LBL("anaLbl")) l->move(cx - 22, anaY);
    m_bindingButtons["Analog"]->setGeometry(cx - row3W/2, anaY + 18, row3W, kBtnH);
}
