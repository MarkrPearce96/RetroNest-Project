#include "pcsx2_graphics_osd_page.h"
#include "../pcsx2_settings_dialog.h"
#include "../pcsx2_theme.h"
#include "../widgets/pcsx2_card.h"
#include "../widgets/pcsx2_combo_row.h"
#include "../widgets/pcsx2_toggle_row.h"
#include "../widgets/pcsx2_slider_row.h"
#include "../widgets/pcsx2_osd_preview.h"
#include "../widgets/pcsx2_toggle.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QVariantMap>
#include <QScrollArea>
#include <QFrame>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QAbstractItemView>
#include <limits>

Pcsx2GraphicsOsdPage::Pcsx2GraphicsOsdPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "OSD")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    syncPreview();
    qApp->installEventFilter(this);
}

Pcsx2GraphicsOsdPage::~Pcsx2GraphicsOsdPage() {
    qApp->removeEventFilter(this);
}

const SettingDef* Pcsx2GraphicsOsdPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsOsdPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void Pcsx2GraphicsOsdPage::buildUi() {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }"
        "QScrollBar::handle:vertical { background: #706c66; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: #7a7670; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::sub-page:vertical, QScrollBar::add-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(12);
    buildLeftCompoundCard(topRow);
    buildRightPreviewCard(topRow);
    root->addLayout(topRow);

    buildBottomToggleGrid(root);
    root->addStretch();
}

void Pcsx2GraphicsOsdPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    auto* card = new Pcsx2Card(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setMinimumHeight(460);

    if (const SettingDef* perfDef = findDef("OsdPerformancePos"))
        card->setSettingDef(*perfDef);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);

    auto addMiniHeader = [&](const QString& text) {
        auto* hdr = new QLabel(text, card);
        hdr->setStyleSheet(
            "color:#f59e0b; font-size:11px; font-weight:700;"
            "letter-spacing:1.0px; padding:6px 0 2px 0;");
        v->addWidget(hdr);
    };

    auto addToggle = [&](const QString& key) {
        const SettingDef* d = findDef(key);
        if (!d) return;
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (!m_preview) return;
            if      (key == "OsdShowFPS")        m_preview->setShowFps(on);
            else if (key == "OsdShowSpeed")      m_preview->setShowSpeed(on);
            else if (key == "OsdShowVPS")        m_preview->setShowVps(on);
            else if (key == "OsdShowResolution") m_preview->setShowResolution(on);
            else if (key == "OsdShowCPU")        m_preview->setShowCpu(on);
            else if (key == "OsdShowGPU")        m_preview->setShowGpu(on);
            else if (key == "OsdShowSettings")   m_preview->setShowSettings(on);
            else if (key == "OsdshowPatches")    m_preview->setShowPatches(on);
            else if (key == "OsdShowInputs")     m_preview->setShowInputs(on);
        });
        v->addWidget(row);
    };

    addMiniHeader(QStringLiteral("PERFORMANCE STATS"));
    addToggle("OsdShowFPS");
    addToggle("OsdShowSpeed");
    addToggle("OsdShowGPU");
    addToggle("OsdShowCPU");
    addToggle("OsdShowResolution");
    addToggle("OsdShowVPS");

    addMiniHeader(QStringLiteral("SETTINGS & INPUTS"));
    addToggle("OsdshowPatches");
    addToggle("OsdShowSettings");
    addToggle("OsdShowInputs");

    v->addStretch();
    topRow->addWidget(card, 1);
}

void Pcsx2GraphicsOsdPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    auto* card = new Pcsx2Card(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setMinimumHeight(460);
    card->setPreviewStyle(true);
    if (const SettingDef* d = findDef("OsdScale"))
        card->setSettingDef(*d);
    connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    auto* lbl = new QLabel(QStringLiteral("OSD PREVIEW"), card);
    lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;"
                       "letter-spacing:0.8px;");
    v->addWidget(lbl);

    m_preview = new Pcsx2OsdPreview(card);
    v->addWidget(m_preview);

    if (const SettingDef* d = findDef("OsdScale")) {
        m_scaleSlider = new Pcsx2SliderRow(card);
        m_scaleSlider->setLabel(d->label);
        m_scaleSlider->setRange(int(d->minVal), int(d->maxVal));
        m_scaleSlider->setSuffix(d->suffix);
        m_scaleSlider->setSettingDef(*d);
        connect(m_scaleSlider, &Pcsx2SliderRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(m_scaleSlider, &Pcsx2SliderRow::valueChanged, this, [this](int val) {
            const SettingDef* dd = findDef("OsdScale");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview) m_preview->setOsdScale(val);
        });
        v->addWidget(m_scaleSlider);
    }

    auto* comboRow = new QHBoxLayout();
    comboRow->setSpacing(8);

    auto addPosCombo = [&](const QString& key, bool drivePerfPreview) -> Pcsx2ComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new Pcsx2ComboRow(card, /*stacked=*/false);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &Pcsx2ComboRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ComboRow::valueChanged, this,
                [this, key, drivePerfPreview](const QString& val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
            if (drivePerfPreview && m_preview)
                m_preview->setPerformancePos(Pcsx2OsdPreview::fromPosValue(val));
        });
        comboRow->addWidget(row, 1);
        return row;
    };

    m_messagesPosCombo = addPosCombo("OsdMessagesPos",    /*drivePerfPreview=*/false);
    m_perfPosCombo     = addPosCombo("OsdPerformancePos", /*drivePerfPreview=*/true);

    v->addLayout(comboRow);
    v->addStretch();
    topRow->addWidget(card, 1);
}

void Pcsx2GraphicsOsdPage::buildBottomToggleGrid(QVBoxLayout* root) {
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        auto* card = new Pcsx2Card(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        const SettingDef* d = findDef(key);
        if (!d) return card;
        card->setSettingDef(*d);

        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (!m_preview) return;
            if      (key == "OsdShowFrameTimes")          m_preview->setShowFrameTimes(on);
            else if (key == "OsdShowIndicators")          m_preview->setShowIndicators(on);
            else if (key == "OsdShowGSStats")             m_preview->setShowGsStats(on);
            else if (key == "OsdShowHardwareInfo")        m_preview->setShowHardwareInfo(on);
            else if (key == "OsdShowVersion")             m_preview->setShowVersion(on);
            else if (key == "OsdShowVideoCapture")        m_preview->setShowVideoCapture(on);
            else if (key == "OsdShowInputRec")            m_preview->setShowInputRec(on);
            else if (key == "OsdShowTextureReplacements") m_preview->setShowTextureReplacements(on);
        });
        v->addWidget(row);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setSpacing(12);

    grid->addWidget(makeToggleCard("OsdShowFrameTimes"),           0, 0);
    grid->addWidget(makeToggleCard("OsdShowIndicators"),           0, 1);
    grid->addWidget(makeToggleCard("OsdShowGSStats"),              0, 2);
    grid->addWidget(makeToggleCard("OsdShowHardwareInfo"),         1, 0);
    grid->addWidget(makeToggleCard("OsdShowVersion"),              1, 1);
    grid->addWidget(makeToggleCard("OsdShowVideoCapture"),         1, 2);
    grid->addWidget(makeToggleCard("OsdShowInputRec"),             2, 0);
    grid->addWidget(makeToggleCard("OsdShowTextureReplacements"),  2, 1);
    if (findDef("WarnAboutUnsafeSettings"))
        grid->addWidget(makeToggleCard("WarnAboutUnsafeSettings"), 2, 2);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    root->addLayout(grid);
}

void Pcsx2GraphicsOsdPage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = tog->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        tog->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        bool ok = false;
        int v = cur.toInt(&ok);
        if (!ok) v = d.defaultValue.toInt();
        slider->setValue(v);
    }
}

void Pcsx2GraphicsOsdPage::syncPreview() {
    if (!m_preview) return;

    if (m_perfPosCombo) {
        m_preview->setPerformancePos(
            Pcsx2OsdPreview::fromPosValue(m_perfPosCombo->value()));
    }
    if (m_scaleSlider)
        m_preview->setOsdScale(m_scaleSlider->value());

    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        const QString& key = tog->settingDef().key;
        const bool on = tog->isChecked();
        if      (key == "OsdShowFPS")                 m_preview->setShowFps(on);
        else if (key == "OsdShowSpeed")               m_preview->setShowSpeed(on);
        else if (key == "OsdShowVPS")                 m_preview->setShowVps(on);
        else if (key == "OsdShowResolution")          m_preview->setShowResolution(on);
        else if (key == "OsdShowCPU")                 m_preview->setShowCpu(on);
        else if (key == "OsdShowGPU")                 m_preview->setShowGpu(on);
        else if (key == "OsdShowGSStats")             m_preview->setShowGsStats(on);
        else if (key == "OsdShowFrameTimes")          m_preview->setShowFrameTimes(on);
        else if (key == "OsdShowHardwareInfo")        m_preview->setShowHardwareInfo(on);
        else if (key == "OsdShowVersion")             m_preview->setShowVersion(on);
        else if (key == "OsdShowIndicators")          m_preview->setShowIndicators(on);
        else if (key == "OsdShowVideoCapture")        m_preview->setShowVideoCapture(on);
        else if (key == "OsdShowInputRec")            m_preview->setShowInputRec(on);
        else if (key == "OsdShowTextureReplacements") m_preview->setShowTextureReplacements(on);
        else if (key == "OsdShowSettings")            m_preview->setShowSettings(on);
        else if (key == "OsdshowPatches")             m_preview->setShowPatches(on);
        else if (key == "OsdShowInputs")              m_preview->setShowInputs(on);
    }
}

bool Pcsx2GraphicsOsdPage::eventFilter(QObject* obj, QEvent* e) {
    Q_UNUSED(obj);
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        const int k = ke->key();
        if (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down) {
            QWidget* current = QApplication::focusWidget();
            if (current && isAncestorOf(current)) {
                if (auto* combo = qobject_cast<QComboBox*>(current)) {
                    if (combo->view() && combo->view()->isVisible()) {
                        return QWidget::eventFilter(obj, e);
                    }
                }
                if (QWidget* next = findNextFocusSpatial(current, k)) {
                    next->setFocus(Qt::TabFocusReason);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, e);
}

QList<QWidget*> Pcsx2GraphicsOsdPage::collectFocusables() const {
    QList<QWidget*> result;
    const auto all = this->findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)   ||
            qobject_cast<QSlider*>(w)     ||
            qobject_cast<QSpinBox*>(w)    ||
            qobject_cast<Pcsx2Toggle*>(w) ||
            qobject_cast<Pcsx2Card*>(w)) {
            result.append(w);
        }
    }
    return result;
}

QWidget* Pcsx2GraphicsOsdPage::findNextFocusSpatial(QWidget* current, int key) const {
    const auto focusables = collectFocusables();
    if (focusables.size() < 2) return nullptr;

    auto pagePoint = [this](QWidget* w) -> QPoint {
        return w->mapTo(const_cast<Pcsx2GraphicsOsdPage*>(this), QPoint(0, 0));
    };
    const QRect myRect(pagePoint(current), current->size());
    const QPoint myCenter = myRect.center();

    QWidget* best = nullptr;
    long long bestScore = std::numeric_limits<long long>::max();

    for (QWidget* w : focusables) {
        if (w == current) continue;
        const QRect r(pagePoint(w), w->size());
        const QPoint c = r.center();
        const int dx = c.x() - myCenter.x();
        const int dy = c.y() - myCenter.y();

        bool inDir = false;
        switch (key) {
            case Qt::Key_Left:  inDir = dx < 0; break;
            case Qt::Key_Right: inDir = dx > 0; break;
            case Qt::Key_Up:    inDir = dy < 0; break;
            case Qt::Key_Down:  inDir = dy > 0; break;
        }
        if (!inDir) continue;

        const bool vertical = (key == Qt::Key_Up || key == Qt::Key_Down);
        const long long adx = qAbs(dx);
        const long long ady = qAbs(dy);
        const long long score = vertical
            ? (ady * 10000LL + adx)
            : (adx * 10000LL + ady);

        if (score < bestScore) {
            bestScore = score;
            best = w;
        }
    }
    return best;
}
