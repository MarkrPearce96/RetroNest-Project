#include "duckstation_graphics_osd_page.h"
#include "../duckstation_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_section_header.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_slider_row.h"
#include "ui/settings/widgets/settings_toggle.h"
#include "../widgets/duckstation_osd_preview.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/duckstation_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVariantMap>
#include <QFrame>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QAbstractItemView>
#include <limits>

DuckStationGraphicsOsdPage::DuckStationGraphicsOsdPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "On-Screen Display") m_schema.append(d);
    buildUi();
    loadValues();
    syncPreview();
    qApp->installEventFilter(this);
}

DuckStationGraphicsOsdPage::~DuckStationGraphicsOsdPage() {
    qApp->removeEventFilter(this);
}

const SettingDef* DuckStationGraphicsOsdPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationGraphicsOsdPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void DuckStationGraphicsOsdPage::buildUi() {
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

void DuckStationGraphicsOsdPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    auto* card = new SettingsCard(this);
    card->setFocusPolicy(Qt::NoFocus);
    if (const SettingDef* d = findDef("ShowFPS"))
        card->setSettingDef(*d);
    connect(card, &SettingsCard::focused, this, &DuckStationGraphicsOsdPage::settingFocused);

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
        connect(row, &SettingsToggleRow::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (!m_preview) return;
            // Only the eight perf-stats toggles are added through this lambda.
            // The four message/input toggles live in buildBottomToggleGrid.
            if      (key == "ShowFPS")               m_preview->setShowFPS(on);
            else if (key == "ShowSpeed")             m_preview->setShowSpeed(on);
            else if (key == "ShowCPU")               m_preview->setShowCPU(on);
            else if (key == "ShowGPU")               m_preview->setShowGPU(on);
            else if (key == "ShowResolution")        m_preview->setShowResolution(on);
            else if (key == "ShowGPUStatistics")     m_preview->setShowGPUStatistics(on);
            else if (key == "ShowFrameTimes")        m_preview->setShowFrameTimes(on);
            else if (key == "ShowLatencyStatistics") m_preview->setShowLatencyStatistics(on);
        });
        v->addWidget(row);
    };

    addMiniHeader(QStringLiteral("PERFORMANCE STATS"));
    addToggle("ShowFPS");
    addToggle("ShowSpeed");
    addToggle("ShowCPU");
    addToggle("ShowGPU");
    addToggle("ShowResolution");
    addToggle("ShowGPUStatistics");
    addToggle("ShowFrameTimes");
    addToggle("ShowLatencyStatistics");

    v->addStretch();
    topRow->addWidget(card, 1);
}

void DuckStationGraphicsOsdPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    auto* card = new SettingsCard(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setPreviewStyle(true);
    if (const SettingDef* d = findDef("OSDScale")) card->setSettingDef(*d);
    connect(card, &SettingsCard::focused, this, &DuckStationGraphicsOsdPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    auto* lbl = new QLabel(QStringLiteral("OSD PREVIEW"), card);
    lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;letter-spacing:0.8px;");
    v->addWidget(lbl);

    m_preview = new DuckStationOsdPreview(card);
    v->addWidget(m_preview);

    if (const SettingDef* d = findDef("OSDScale")) {
        m_scaleSlider = new SettingsSliderRow(card);
        m_scaleSlider->setLabel(d->label);
        m_scaleSlider->setRange(int(d->minVal), int(d->maxVal));
        m_scaleSlider->setSuffix(d->suffix);
        m_scaleSlider->setSettingDef(*d);
        connect(m_scaleSlider, &SettingsSliderRow::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(m_scaleSlider, &SettingsSliderRow::valueChanged, this, [this](int val) {
            const SettingDef* dd = findDef("OSDScale");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview) m_preview->setScale(val);
        });
        v->addWidget(m_scaleSlider);
    }

    if (const SettingDef* d = findDef("OSDMargin")) {
        m_marginSlider = new SettingsSliderRow(card);
        m_marginSlider->setLabel(d->label);
        m_marginSlider->setRange(int(d->minVal), int(d->maxVal));
        m_marginSlider->setSuffix(d->suffix);
        m_marginSlider->setSettingDef(*d);
        connect(m_marginSlider, &SettingsSliderRow::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(m_marginSlider, &SettingsSliderRow::valueChanged, this, [this](int val) {
            const SettingDef* dd = findDef("OSDMargin");
            if (dd) saveValue(dd->section, dd->key, QString::number(val));
            if (m_preview) m_preview->setMargin(val);
        });
        v->addWidget(m_marginSlider);
    }

    if (const SettingDef* d = findDef("OSDMessageLocation")) {
        m_locationCombo = new SettingsComboRow(card, /*stacked=*/false);
        m_locationCombo->setLabel(d->label);
        m_locationCombo->setOptions(d->options);
        m_locationCombo->setSettingDef(*d);
        connect(m_locationCombo, &SettingsComboRow::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(m_locationCombo, &SettingsComboRow::valueChanged, this, [this](const QString& val) {
            const SettingDef* dd = findDef("OSDMessageLocation");
            if (dd) saveValue(dd->section, dd->key, val);
            if (m_preview) m_preview->setMessageLocation(val);
        });
        v->addWidget(m_locationCombo);
    }

    v->addStretch();
    topRow->addWidget(card, 1);
}

void DuckStationGraphicsOsdPage::buildBottomToggleGrid(QVBoxLayout* root) {
    auto makeToggleCard = [this](const QString& key) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new SettingsCard(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);

        auto* row = new SettingsToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(row, &SettingsToggleRow::focused, this, &DuckStationGraphicsOsdPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
            if (!m_preview) return;
            if      (key == "ShowOSDMessages")      m_preview->setShowOSDMessages(on);
            else if (key == "ShowStatusIndicators") m_preview->setShowStatusIndicators(on);
            else if (key == "ShowInputs")           m_preview->setShowInputs(on);
            else if (key == "ShowEnhancements")     m_preview->setShowEnhancements(on);
        });
        v->addWidget(row);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setSpacing(12);

    int row = 0, col = 0;
    auto add = [&](const QString& key) {
        auto* w = makeToggleCard(key);
        if (!w) return;
        grid->addWidget(w, row, col);
        if (++col == 3) { col = 0; ++row; }
    };
    // Messages & Inputs visibility — moved out of the left compound card so
    // they line up alongside Animate Messages / Blur Message Backgrounds.
    add("ShowOSDMessages");
    add("ShowStatusIndicators");
    add("ShowInputs");
    add("ShowEnhancements");
    add("AnimateOSDMessages");
    add("BlurOSDMessageBackgrounds");

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    root->addLayout(grid);
}

void DuckStationGraphicsOsdPage::loadValues() {
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

void DuckStationGraphicsOsdPage::syncPreview() {
    if (!m_preview) return;
    if (m_scaleSlider)   m_preview->setScale(m_scaleSlider->value());
    if (m_marginSlider)  m_preview->setMargin(m_marginSlider->value());
    if (m_locationCombo) m_preview->setMessageLocation(m_locationCombo->value());

    for (auto* tog : findChildren<SettingsToggleRow*>()) {
        const QString& key = tog->settingDef().key;
        const bool on = tog->isChecked();
        if      (key == "ShowFPS")               m_preview->setShowFPS(on);
        else if (key == "ShowSpeed")             m_preview->setShowSpeed(on);
        else if (key == "ShowCPU")               m_preview->setShowCPU(on);
        else if (key == "ShowGPU")               m_preview->setShowGPU(on);
        else if (key == "ShowResolution")        m_preview->setShowResolution(on);
        else if (key == "ShowGPUStatistics")     m_preview->setShowGPUStatistics(on);
        else if (key == "ShowFrameTimes")        m_preview->setShowFrameTimes(on);
        else if (key == "ShowLatencyStatistics") m_preview->setShowLatencyStatistics(on);
        else if (key == "ShowOSDMessages")       m_preview->setShowOSDMessages(on);
        else if (key == "ShowStatusIndicators")  m_preview->setShowStatusIndicators(on);
        else if (key == "ShowInputs")            m_preview->setShowInputs(on);
        else if (key == "ShowEnhancements")      m_preview->setShowEnhancements(on);
    }
}

bool DuckStationGraphicsOsdPage::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        const int k = ke->key();
        if (k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down) {
            QWidget* current = QApplication::focusWidget();
            if (current && isAncestorOf(current)) {
                if (auto* combo = qobject_cast<QComboBox*>(current))
                    if (combo->view() && combo->view()->isVisible())
                        return QWidget::eventFilter(obj, e);
                if (current->property("editing").toBool())
                    return QWidget::eventFilter(obj, e);
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

QList<QWidget*> DuckStationGraphicsOsdPage::collectFocusables() const {
    QList<QWidget*> result;
    const auto all = this->findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)   ||
            qobject_cast<QSlider*>(w)     ||
            qobject_cast<SettingsToggle*>(w) ||
            qobject_cast<SettingsCard*>(w)) {
            // Skip controls that live inside a focusable SettingsCard — the card
            // is the focus stop and Enter activates the inner widget.
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

QWidget* DuckStationGraphicsOsdPage::findNextFocusSpatial(QWidget* current, int key) const {
    const auto focusables = collectFocusables();
    if (focusables.size() < 2) return nullptr;

    auto pagePoint = [this](QWidget* w) -> QPoint {
        return w->mapTo(const_cast<DuckStationGraphicsOsdPage*>(this), QPoint(0, 0));
    };
    const QRect mine(pagePoint(current), current->size());
    const QPoint myCenter = mine.center();
    const bool vertical = (key == Qt::Key_Up || key == Qt::Key_Down);

    auto rangesOverlap = [](int a0, int a1, int b0, int b1) {
        return a0 < b1 && b0 < a1;
    };

    QWidget* bestOverlap = nullptr;
    long long bestOverlapScore = std::numeric_limits<long long>::max();

    for (QWidget* w : focusables) {
        if (w == current) continue;
        const QRect r(pagePoint(w), w->size());
        const QPoint c = r.center();
        const int dx = c.x() - myCenter.x();
        const int dy = c.y() - myCenter.y();

        bool inDir = false, perpOverlap = false;
        switch (key) {
            case Qt::Key_Left:  inDir = dx < 0; perpOverlap = rangesOverlap(mine.top(),  mine.bottom(), r.top(),  r.bottom()); break;
            case Qt::Key_Right: inDir = dx > 0; perpOverlap = rangesOverlap(mine.top(),  mine.bottom(), r.top(),  r.bottom()); break;
            case Qt::Key_Up:    inDir = dy < 0; perpOverlap = rangesOverlap(mine.left(), mine.right(),  r.left(), r.right());  break;
            case Qt::Key_Down:  inDir = dy > 0; perpOverlap = rangesOverlap(mine.left(), mine.right(),  r.left(), r.right());  break;
        }
        if (!inDir) continue;

        long long adx = qAbs(dx), ady = qAbs(dy);
        if (perpOverlap) {
            if (vertical) {
                const int oL = qMax(mine.left(), r.left());
                const int oR = qMin(mine.right(), r.right());
                adx = qAbs((oL + oR) / 2 - myCenter.x());
            } else {
                const int oT = qMax(mine.top(), r.top());
                const int oB = qMin(mine.bottom(), r.bottom());
                ady = qAbs((oT + oB) / 2 - myCenter.y());
            }
        }
        const long long score = vertical ? (ady * 2LL + adx) : (adx * 2LL + ady);
        if (perpOverlap && score < bestOverlapScore) {
            bestOverlapScore = score;
            bestOverlap = w;
        }
    }
    return bestOverlap;
}
