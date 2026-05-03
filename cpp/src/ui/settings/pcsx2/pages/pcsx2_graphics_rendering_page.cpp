#include "pcsx2_graphics_rendering_page.h"
#include "../pcsx2_settings_dialog.h"
#include "ui/settings/settings_dialog_theme.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_toggle.h"
#include "ui/app_controller.h"
#include "adapters/pcsx2_adapter.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QVariantMap>
#include <QScrollArea>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QAbstractItemView>
#include <limits>

Pcsx2GraphicsRenderingPage::Pcsx2GraphicsRenderingPage(Pcsx2SettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    PCSX2Adapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" && d.subcategory == "Rendering")
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    qApp->installEventFilter(this);
}

Pcsx2GraphicsRenderingPage::~Pcsx2GraphicsRenderingPage() {
    qApp->removeEventFilter(this);
}

const SettingDef* Pcsx2GraphicsRenderingPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void Pcsx2GraphicsRenderingPage::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 12, 24, 16);
    root->setSpacing(12);

    auto makeComboCard = [this](const QString& key) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        auto* card = new SettingsCard(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) return card;
        card->setSettingDef(*d);
        auto* row = new SettingsComboRow(card, /*stacked=*/true);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &SettingsComboRow::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &SettingsComboRow::valueChanged, this, [this, key](const QString& val){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return card;
    };

    auto makeToggleCard = [this](const QString& key) -> SettingsCard* {
        const SettingDef* d = findDef(key);
        auto* card = new SettingsCard(this);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        if (!d) return card;
        card->setSettingDef(*d);
        auto* row = new SettingsToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &SettingsCard::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &SettingsToggleRow::focused, this, &Pcsx2GraphicsRenderingPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on){
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    // Full-width Internal Resolution card
    root->addWidget(makeComboCard("upscale_multiplier"));

    // 2-column grid of 6 cards
    auto* grid = new QGridLayout();
    grid->setSpacing(12);
    grid->addWidget(makeComboCard("filter"),                 0, 0);
    grid->addWidget(makeComboCard("TriFilter"),              0, 1);
    grid->addWidget(makeComboCard("MaxAnisotropy"),          1, 0);
    grid->addWidget(makeComboCard("dithering_ps2"),          1, 1);
    grid->addWidget(makeComboCard("accurate_blending_unit"), 2, 0);
    grid->addWidget(makeToggleCard("hw_mipmap"),             2, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    root->addLayout(grid);
    root->addStretch();
}

void Pcsx2GraphicsRenderingPage::loadValues() {
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
}

void Pcsx2GraphicsRenderingPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

bool Pcsx2GraphicsRenderingPage::eventFilter(QObject* obj, QEvent* e) {
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
                    QWidget* p = next->parentWidget();
                    while (p) {
                        if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                            sa->ensureWidgetVisible(next, 20, 40);
                            break;
                        }
                        p = p->parentWidget();
                    }
                }
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, e);
}

QList<QWidget*> Pcsx2GraphicsRenderingPage::collectFocusables() const {
    QList<QWidget*> result;
    const auto all = this->findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)   ||
            qobject_cast<QSlider*>(w)     ||
            qobject_cast<SettingsToggle*>(w) ||
            qobject_cast<SettingsCard*>(w)) {
            // If this control lives inside a focusable SettingsCard, skip it.
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

QWidget* Pcsx2GraphicsRenderingPage::findNextFocusSpatial(QWidget* current, int key) const {
    const auto focusables = collectFocusables();
    if (focusables.size() < 2) return nullptr;

    auto pagePoint = [this](QWidget* w) -> QPoint {
        return w->mapTo(const_cast<Pcsx2GraphicsRenderingPage*>(this), QPoint(0, 0));
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
