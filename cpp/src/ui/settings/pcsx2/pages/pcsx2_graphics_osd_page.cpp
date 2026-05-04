#include "pcsx2_graphics_osd_page.h"
#include "../pcsx2_settings_dialog.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_slider_row.h"
#include "ui/settings/widgets/preview/osd_preview.h"
#include "ui/settings/widgets/settings_toggle.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
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
    scroll->setStyleSheet(SettingsPageBuilder::kScrollAreaQss);
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
    auto* card = new SettingsCard(this);
    card->setFocusPolicy(Qt::NoFocus);

    if (const SettingDef* perfDef = findDef("OsdPerformancePos"))
        card->setSettingDef(*perfDef);
    connect(card, &SettingsCard::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);

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
        auto* row = new SettingsToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &SettingsToggleRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on) {
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
    auto* card = new SettingsCard(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setPreviewStyle(true);
    if (const SettingDef* d = findDef("OsdScale"))
        card->setSettingDef(*d);
    connect(card, &SettingsCard::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    auto* lbl = new QLabel(QStringLiteral("OSD PREVIEW"), card);
    lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;"
                       "letter-spacing:0.8px;");
    v->addWidget(lbl);

    m_preview = new OsdPreview(card);
    v->addWidget(m_preview);

    if (const SettingDef* d = findDef("OsdScale")) {
        m_scaleSlider = new SettingsSliderRow(card);
        m_scaleSlider->setLabel(d->label);
        m_scaleSlider->setRange(int(d->minVal), int(d->maxVal));
        m_scaleSlider->setSuffix(d->suffix);
        m_scaleSlider->setSettingDef(*d);
        connect(m_scaleSlider, &SettingsSliderRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(m_scaleSlider, &SettingsSliderRow::valueChanged, this, [this](int val) {
            const SettingDef* dd = findDef("OsdScale");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview) m_preview->setOsdScale(val);
        });
        v->addWidget(m_scaleSlider);
    }

    auto addPosCombo = [&](const QString& key, bool drivePerfPreview) -> SettingsComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new SettingsComboRow(card, /*stacked=*/false);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &SettingsComboRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &SettingsComboRow::valueChanged, this,
                [this, key, drivePerfPreview](const QString& val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
            if (drivePerfPreview && m_preview)
                m_preview->setPerformancePos(OsdPreview::fromPosValue(val));
        });
        return row;
    };

    m_messagesPosCombo = addPosCombo("OsdMessagesPos",    /*drivePerfPreview=*/false);
    m_perfPosCombo     = addPosCombo("OsdPerformancePos", /*drivePerfPreview=*/true);

    if (m_messagesPosCombo) v->addWidget(m_messagesPosCombo);
    if (m_perfPosCombo)     v->addWidget(m_perfPosCombo);
    v->addStretch();
    topRow->addWidget(card, 1);
}

void Pcsx2GraphicsOsdPage::buildBottomToggleGrid(QVBoxLayout* root) {
    auto makeToggleCard = [this](const QString& key) -> SettingsCard* {
        auto* card = new SettingsCard(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        const SettingDef* d = findDef(key);
        if (!d) return card;
        card->setSettingDef(*d);

        auto* row = new SettingsToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &SettingsToggleRow::focused, this, &Pcsx2GraphicsOsdPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on) {
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

    for (auto* combo : findChildren<SettingsComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* tog : findChildren<SettingsToggleRow*>()) {
        const SettingDef& d = tog->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        tog->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<SettingsSliderRow*>()) {
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
            OsdPreview::fromPosValue(m_perfPosCombo->value()));
    }
    if (m_scaleSlider)
        m_preview->setOsdScale(m_scaleSlider->value());

    for (auto* tog : findChildren<SettingsToggleRow*>()) {
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
                // Sliders and spin boxes in edit mode handle their own arrows.
                if (current->property("editing").toBool()) {
                    return QWidget::eventFilter(obj, e);
                }
                if (QWidget* next = findNextFocusSpatial(current, k)) {
                    next->setFocus(Qt::TabFocusReason);
                    for (QWidget* p = next->parentWidget(); p; p = p->parentWidget()) {
                        if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                            sa->ensureWidgetVisible(next, 20, 40);
                            break;
                        }
                    }
                }
                return true;
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
            qobject_cast<SettingsToggle*>(w) ||
            qobject_cast<SettingsCard*>(w)) {
            // If this control lives inside a focusable SettingsCard, skip it —
            // the card itself is the focus stop and Enter activates the control.
            if (!qobject_cast<SettingsCard*>(w)) {
                bool insideFocusableCard = false;
                for (QWidget* p = w->parentWidget(); p && p != this; p = p->parentWidget()) {
                    if (auto* card = qobject_cast<SettingsCard*>(p)) {
                        if (card->focusPolicy() != Qt::NoFocus) {
                            insideFocusableCard = true;
                            break;
                        }
                    }
                }
                if (insideFocusableCard) continue;
            }
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
    const QRect mine(pagePoint(current), current->size());
    const QPoint myCenter = mine.center();
    const bool vertical = (key == Qt::Key_Up || key == Qt::Key_Down);

    auto rangesOverlap = [](int a0, int a1, int b0, int b1) {
        return a0 < b1 && b0 < a1;
    };

    QWidget* bestOverlap = nullptr;  long long bestOverlapScore = std::numeric_limits<long long>::max();

    for (QWidget* w : focusables) {
        if (w == current) continue;
        const QRect r(pagePoint(w), w->size());
        const QPoint c = r.center();
        const int dx = c.x() - myCenter.x();
        const int dy = c.y() - myCenter.y();

        bool inDir = false;
        bool perpOverlap = false;
        switch (key) {
            case Qt::Key_Left:
                inDir = dx < 0;
                perpOverlap = rangesOverlap(mine.top(), mine.bottom(), r.top(), r.bottom());
                break;
            case Qt::Key_Right:
                inDir = dx > 0;
                perpOverlap = rangesOverlap(mine.top(), mine.bottom(), r.top(), r.bottom());
                break;
            case Qt::Key_Up:
                inDir = dy < 0;
                perpOverlap = rangesOverlap(mine.left(), mine.right(), r.left(), r.right());
                break;
            case Qt::Key_Down:
                inDir = dy > 0;
                perpOverlap = rangesOverlap(mine.left(), mine.right(), r.left(), r.right());
                break;
        }
        if (!inDir) continue;

        long long adx = qAbs(dx);
        long long ady = qAbs(dy);
        if (perpOverlap) {
            if (vertical) {
                int oL = qMax(mine.left(), r.left());
                int oR = qMin(mine.right(), r.right());
                adx = qAbs((oL + oR) / 2 - myCenter.x());
            } else {
                int oT = qMax(mine.top(), r.top());
                int oB = qMin(mine.bottom(), r.bottom());
                ady = qAbs((oT + oB) / 2 - myCenter.y());
            }
        }
        const long long score = vertical
            ? (ady * 2LL + adx)
            : (adx * 2LL + ady);

        if (perpOverlap && score < bestOverlapScore) {
            bestOverlapScore = score;
            bestOverlap = w;
        }
    }
    return bestOverlap;
}
