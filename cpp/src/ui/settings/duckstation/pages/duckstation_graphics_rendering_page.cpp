#include "duckstation_graphics_rendering_page.h"
#include "../duckstation_settings_dialog.h"
#include "ui/settings/widgets/settings_card.h"
#include "ui/settings/widgets/settings_combo_row.h"
#include "ui/settings/widgets/settings_toggle_row.h"
#include "ui/settings/widgets/settings_slider_row.h"
#include "ui/settings/widgets/settings_toggle.h"
#include "../widgets/duckstation_aspect_ratio_preview.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_page_builder.h"
#include "adapters/duckstation_adapter.h"
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
#include <QAbstractItemView>
#include <QSlider>
#include <limits>

DuckStationGraphicsRenderingPage::DuckStationGraphicsRenderingPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema()) {
        if (d.category == "Graphics" &&
            (d.subcategory.isEmpty() || d.subcategory == "Rendering"))
            m_schema.append(d);
    }
    buildUi();
    loadValues();
    syncPreview();
    qApp->installEventFilter(this);
}

DuckStationGraphicsRenderingPage::~DuckStationGraphicsRenderingPage() {
    qApp->removeEventFilter(this);
}

const SettingDef* DuckStationGraphicsRenderingPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationGraphicsRenderingPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void DuckStationGraphicsRenderingPage::buildUi() {
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

    // Remaining combo cards
    SettingsPageBuilder builder(this, m_schema,
        [this](const QString& sec, const QString& k, const QString& v){ saveValue(sec, k, v); },
        [this](const SettingDef& d){ emit settingFocused(d); });
    auto makeComboCard = [&builder](const QString& key){ return builder.makeComboCard(key); };

    if (auto* c = makeComboCard("ResolutionScale"))     root->addWidget(c);
    if (auto* c = makeComboCard("DownsampleMode"))      root->addWidget(c);
    if (auto* c = makeComboCard("TextureFilter"))       root->addWidget(c);
    if (auto* c = makeComboCard("SpriteTextureFilter")) root->addWidget(c);
    if (auto* c = makeComboCard("DitheringMode"))       root->addWidget(c);

    root->addStretch();
}

void DuckStationGraphicsRenderingPage::buildLeftCompoundCard(QHBoxLayout* topRow) {
    auto* card = new SettingsCard(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setMinimumHeight(460);

    if (const SettingDef* arDef = findDef("AspectRatio"))
        card->setSettingDef(*arDef);
    connect(card, &SettingsCard::focused, this, &DuckStationGraphicsRenderingPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(8);

    auto addCombo = [&](const QString& key) -> SettingsComboRow* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* row = new SettingsComboRow(card, /*stacked=*/true);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &SettingsComboRow::focused, this, &DuckStationGraphicsRenderingPage::settingFocused);
        connect(row, &SettingsComboRow::valueChanged, this, [this, key](const QString& val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
        return row;
    };

    addCombo("Renderer");
    addCombo("Adapter");
    m_aspectCombo = addCombo("AspectRatio");
    addCombo("DeinterlacingMode");
    addCombo("Scaling");

    if (const SettingDef* d = findDef("WidescreenHack")) {
        auto* row = new SettingsToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(row, &SettingsToggleRow::focused, this, &DuckStationGraphicsRenderingPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this](bool on) {
            const SettingDef* dd = findDef("WidescreenHack");
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
    }

    if (m_aspectCombo) {
        connect(m_aspectCombo, &SettingsComboRow::valueChanged, this, [this](const QString& val) {
            if (m_preview) m_preview->setAspectRatio(
                DuckStationAspectRatioPreview::fromSchemaValue(val));
        });
    }

    topRow->addWidget(card, 1);
}

void DuckStationGraphicsRenderingPage::buildRightPreviewCard(QHBoxLayout* topRow) {
    auto* card = new SettingsCard(this);
    card->setFocusPolicy(Qt::NoFocus);
    card->setMinimumHeight(460);
    card->setPreviewStyle(true);
    connect(card, &SettingsCard::focused, this, &DuckStationGraphicsRenderingPage::settingFocused);

    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(14, 12, 14, 12);
    v->setSpacing(10);

    auto* lbl = new QLabel(QStringLiteral("ASPECT RATIO PREVIEW"), card);
    lbl->setStyleSheet("color:#9a9690;font-size:11px;font-weight:600;"
                       "letter-spacing:0.8px;");
    v->addWidget(lbl);

    m_preview = new DuckStationAspectRatioPreview(card);
    v->addWidget(m_preview);

    auto addCombo = [&](const QString& key) {
        const SettingDef* d = findDef(key);
        if (!d) return;
        auto* row = new SettingsComboRow(card, /*stacked=*/true);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(row, &SettingsComboRow::focused, this, &DuckStationGraphicsRenderingPage::settingFocused);
        connect(row, &SettingsComboRow::valueChanged, this, [this, key](const QString& val) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, val);
        });
        v->addWidget(row);
    };

    addCombo("CropMode");
    addCombo("Scaling24Bit");

    topRow->addWidget(card, 1);
}

void DuckStationGraphicsRenderingPage::buildBottomToggleGrid(QVBoxLayout* root) {
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
        connect(card, &SettingsCard::focused, this, &DuckStationGraphicsRenderingPage::settingFocused);
        connect(row, &SettingsToggleRow::focused, this, &DuckStationGraphicsRenderingPage::settingFocused);
        connect(row, &SettingsToggleRow::toggled, this, [this, key](bool on) {
            const SettingDef* dd = findDef(key);
            if (dd) saveValue(dd->section, dd->key, on ? "true" : "false");
        });
        v->addWidget(row);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setSpacing(12);

    grid->addWidget(makeToggleCard("PGXPEnable"),                   0, 0);
    grid->addWidget(makeToggleCard("PGXPDepthBuffer"),              0, 1);
    grid->addWidget(makeToggleCard("Force4_3For24Bit"),             0, 2);
    grid->addWidget(makeToggleCard("ChromaSmoothing24Bit"),         1, 0);
    grid->addWidget(makeToggleCard("ForceRoundTextureCoordinates"), 1, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    root->addLayout(grid);
}

void DuckStationGraphicsRenderingPage::loadValues() {
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

void DuckStationGraphicsRenderingPage::syncPreview() {
    if (!m_preview || !m_aspectCombo) return;
    m_preview->setAspectRatio(
        DuckStationAspectRatioPreview::fromSchemaValue(m_aspectCombo->value()));
}

bool DuckStationGraphicsRenderingPage::eventFilter(QObject* obj, QEvent* e) {
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

QList<QWidget*> DuckStationGraphicsRenderingPage::collectFocusables() const {
    QList<QWidget*> result;
    const auto all = this->findChildren<QWidget*>();
    for (QWidget* w : all) {
        if (!w->isVisible()) continue;
        if (w->focusPolicy() == Qt::NoFocus) continue;
        if (qobject_cast<QComboBox*>(w)   ||
            qobject_cast<QSlider*>(w)     ||
            qobject_cast<SettingsToggle*>(w) ||
            qobject_cast<SettingsCard*>(w)) {
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

QWidget* DuckStationGraphicsRenderingPage::findNextFocusSpatial(QWidget* current, int key) const {
    const auto focusables = collectFocusables();
    if (focusables.size() < 2) return nullptr;

    auto pagePoint = [this](QWidget* w) -> QPoint {
        return w->mapTo(const_cast<DuckStationGraphicsRenderingPage*>(this), QPoint(0, 0));
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
