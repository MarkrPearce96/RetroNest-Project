#include "duckstation_console_page.h"
#include "../duckstation_settings_dialog.h"
#include "../../pcsx2/widgets/pcsx2_card.h"
#include "../../pcsx2/widgets/pcsx2_section_header.h"
#include "../../pcsx2/widgets/pcsx2_combo_row.h"
#include "../../pcsx2/widgets/pcsx2_toggle_row.h"
#include "../../pcsx2/widgets/pcsx2_slider_row.h"
#include "ui/app_controller.h"
#include "adapters/duckstation_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QVariantMap>
#include <QGraphicsOpacityEffect>
#include <QHash>
#include <QSignalBlocker>
#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QComboBox>
#include <QSlider>
#include <QAbstractItemView>
#include <functional>
#include <numeric>
#include <limits>

DuckStationConsolePage::DuckStationConsolePage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Console") m_schema.append(d);
    buildUi();
    loadValues();
    refreshDependencies();
    // Page-level arrow-key spatial nav. Pcsx2Card's own keyPressEvent only
    // looks at direct sibling cards, which misses cards nested inside grid
    // sub-layouts (e.g. the OverclockEnable / RecompilerICache row sitting
    // below the full-width Clock Speed Multiplier slider). The app-level
    // filter uses mapTo(this) so geometry comparisons work regardless of
    // each card's parent widget.
    qApp->installEventFilter(this);
}

DuckStationConsolePage::~DuckStationConsolePage() {
    qApp->removeEventFilter(this);
}

const SettingDef* DuckStationConsolePage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationConsolePage::buildUi() {
    // Outer layout — just hosts the scroll area
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
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(10);

    auto* back = new QPushButton("\u2190 Back", content);
    back->setStyleSheet("QPushButton { background:transparent; color:#f2efe8; border:none;"
                        " font-size:14px; padding:4px 0; text-align:left; }"
                        "QPushButton:focus { color:#f59e0b; }");
    connect(back, &QPushButton::clicked, m_dialog, &DuckStationSettingsDialog::popPage);
    root->addWidget(back);

    auto makeComboCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ComboRow(card);
        row->setLabel(d->label);
        row->setOptions(d->options);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row,  &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& v){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, v);
        });
        v->addWidget(row);
        return card;
    };
    auto makeSliderCard = [this](const QString& key,
                                 std::function<void(int)> onChange = {}) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2SliderRow(card);
        row->setLabel(d->label);
        row->setRange(int(d->minVal), int(d->maxVal));
        row->setSuffix(d->suffix);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row, &Pcsx2SliderRow::focused, this, &DuckStationConsolePage::settingFocused);
        if (onChange) {
            connect(row, &Pcsx2SliderRow::valueChanged, this, onChange);
        } else {
            connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
                if (const SettingDef* d2 = findDef(key))
                    saveValue(d2->section, d2->key, QString::number(val));
            });
        }
        v->addWidget(row);
        return card;
    };
    auto makeToggleCard = [this](const QString& key) -> Pcsx2Card* {
        const SettingDef* d = findDef(key);
        if (!d) return nullptr;
        auto* card = new Pcsx2Card(this);
        card->setSettingDef(*d);
        auto* v = new QVBoxLayout(card);
        v->setContentsMargins(14, 12, 14, 12);
        auto* row = new Pcsx2ToggleRow(card);
        row->setLabel(d->label);
        row->setSettingDef(*d);
        connect(card, &Pcsx2Card::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &DuckStationConsolePage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
            // Toggling overclocking off resets the multiplier back to 100%
            // (numerator/denominator = 1/1) so a stale value can't take effect
            // the next time the user re-enables the toggle.
            if (key == "OverclockEnable" && !on) {
                for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
                    if (slider->settingDef().key == "OverclockNumerator") {
                        QSignalBlocker sb(slider);
                        slider->setValue(100);
                    }
                }
                saveValue("CPU", "OverclockNumerator",   "1");
                saveValue("CPU", "OverclockDenominator", "1");
            }
            refreshDependencies();
        });
        v->addWidget(row);
        return card;
    };
    auto addIfPresent = [](QBoxLayout* layout, QWidget* w){ if (w) layout->addWidget(w); };
    (void)addIfPresent;

    // Console
    root->addWidget(new Pcsx2SectionHeader("Console", this));
    if (auto* c = makeComboCard("Region"))          root->addWidget(c);
    if (auto* c = makeComboCard("ForceVideoTiming")) root->addWidget(c);

    auto* consoleGrid = new QGridLayout();
    consoleGrid->setSpacing(10);
    int cgr = 0, cgc = 0;
    auto addConsoleToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        consoleGrid->addWidget(w, cgr, cgc);
        if (++cgc == 2) { cgc = 0; ++cgr; }
    };
    addConsoleToggle("PatchFastBoot");
    addConsoleToggle("FastForwardBoot");
    addConsoleToggle("FastForwardAccess");
    addConsoleToggle("Enable8MBRAM");
    root->addLayout(consoleGrid);

    // CPU Emulation
    root->addWidget(new Pcsx2SectionHeader("CPU Emulation", this));
    if (auto* c = makeComboCard("ExecutionMode"))      root->addWidget(c);
    // Clock Speed Multiplier — slider in percent (10–1000%, step 5).
    // DuckStation stores this as a GCD-reduced numerator/denominator pair, so
    // the slider's percent value translates to two INI keys on save.
    if (auto* c = makeSliderCard("OverclockNumerator", [this](int percent){
        const int g = std::gcd(percent, 100);
        saveValue("CPU", "OverclockNumerator",   QString::number(percent / g));
        saveValue("CPU", "OverclockDenominator", QString::number(100 / g));
    })) {
        // Replace the default "215%" label with "215% (72.82MHz)", matching
        // DuckStation's upstream label. Base PSX CPU runs at 33.8688 MHz.
        if (auto* slider = c->findChild<Pcsx2SliderRow*>()) {
            slider->setValueFormatter([](int percent){
                constexpr double kPsxClockMHz = 33.8688;
                return QString("%1% (%2MHz)")
                    .arg(percent)
                    .arg(kPsxClockMHz * percent / 100.0, 0, 'f', 2);
            });
        }
        root->addWidget(c);
    }

    auto* cpuGrid = new QGridLayout();
    cpuGrid->setSpacing(10);
    int cpur = 0, cpuc = 0;
    auto addCpuToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        cpuGrid->addWidget(w, cpur, cpuc);
        if (++cpuc == 2) { cpuc = 0; ++cpur; }
    };
    addCpuToggle("OverclockEnable");
    addCpuToggle("RecompilerICache");
    root->addLayout(cpuGrid);

    // CD-ROM Emulation
    root->addWidget(new Pcsx2SectionHeader("CD-ROM Emulation", this));
    if (auto* c = makeComboCard("ReadSpeedup")) root->addWidget(c);
    if (auto* c = makeComboCard("SeekSpeedup")) root->addWidget(c);

    auto* cdromGrid = new QGridLayout();
    cdromGrid->setSpacing(10);
    int cdr = 0, cdc = 0;
    auto addCdromToggle = [&](const QString& key){
        auto* w = makeToggleCard(key);
        if (!w) return;
        cdromGrid->addWidget(w, cdr, cdc);
        if (++cdc == 2) { cdc = 0; ++cdr; }
    };
    addCdromToggle("LoadImageToRAM");
    addCdromToggle("AutoDiscChange");
    addCdromToggle("LoadImagePatches");
    addCdromToggle("IgnoreHostSubcode");
    root->addLayout(cdromGrid);

    root->addStretch();
}

void DuckStationConsolePage::loadValues() {
    auto* app = m_dialog->appController();
    const QString emuId = m_dialog->emuId();

    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        const SettingDef& d = combo->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        combo->setValue(cur.isEmpty() ? d.defaultValue : cur);
    }
    for (auto* row : findChildren<Pcsx2ToggleRow*>()) {
        const SettingDef& d = row->settingDef();
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        row->setChecked(v.compare("true", Qt::CaseInsensitive) == 0);
    }
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        if (d.key == "OverclockNumerator") {
            const QString numStr = app->settingValue(emuId, "CPU", "OverclockNumerator");
            const QString denStr = app->settingValue(emuId, "CPU", "OverclockDenominator");
            const int num = numStr.isEmpty() ? 1 : numStr.toInt();
            int den = denStr.isEmpty() ? 1 : denStr.toInt();
            if (den == 0) den = 1;
            slider->setValue((num * 100) / den);
        } else {
            const QString cur = app->settingValue(emuId, d.section, d.key);
            const QString v = cur.isEmpty() ? d.defaultValue : cur;
            slider->setValue(v.toInt());
        }
    }
}

void DuckStationConsolePage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

bool DuckStationConsolePage::eventFilter(QObject* obj, QEvent* e) {
    if (e->type() != QEvent::KeyPress) return QWidget::eventFilter(obj, e);
    auto* ke = static_cast<QKeyEvent*>(e);
    const int k = ke->key();
    if (k != Qt::Key_Left && k != Qt::Key_Right && k != Qt::Key_Up && k != Qt::Key_Down)
        return QWidget::eventFilter(obj, e);

    QWidget* current = QApplication::focusWidget();
    if (!current || !isAncestorOf(current))
        return QWidget::eventFilter(obj, e);

    // Combo dropdown open — let the popup handle arrows.
    if (auto* combo = qobject_cast<QComboBox*>(current))
        if (combo->view() && combo->view()->isVisible())
            return QWidget::eventFilter(obj, e);

    // Slider in edit mode handles its own arrows (see Pcsx2SliderRow::setEditing).
    if (current->property("editing").toBool())
        return QWidget::eventFilter(obj, e);

    Pcsx2Card* sourceCard = nullptr;
    for (QWidget* w = current; w; w = w->parentWidget()) {
        if (auto* card = qobject_cast<Pcsx2Card*>(w)) { sourceCard = card; break; }
    }
    if (!sourceCard) return QWidget::eventFilter(obj, e);

    if (Pcsx2Card* next = findNextCardSpatial(sourceCard, k)) {
        next->setFocus(Qt::TabFocusReason);
        for (QWidget* p = next->parentWidget(); p; p = p->parentWidget()) {
            if (auto* sa = qobject_cast<QScrollArea*>(p)) {
                sa->ensureWidgetVisible(next, 20, 40);
                break;
            }
        }
    }
    // Always consume so combos/sliders/toggles don't see the arrow themselves.
    return true;
}

Pcsx2Card* DuckStationConsolePage::findNextCardSpatial(Pcsx2Card* current, int key) const {
    auto pagePoint = [this](QWidget* w) -> QPoint {
        return w->mapTo(const_cast<DuckStationConsolePage*>(this), QPoint(0, 0));
    };
    const QRect mine(pagePoint(current), current->size());
    const QPoint myCenter = mine.center();
    const bool vertical = (key == Qt::Key_Up || key == Qt::Key_Down);

    auto rangesOverlap = [](int a0, int a1, int b0, int b1) { return a0 < b1 && b0 < a1; };

    // First pass: collect every in-direction card whose perpendicular range
    // overlaps the source. Track the closest "next row" (or column) by the
    // primary-axis gap so a strictly closer card always wins over a farther
    // but better-aligned one — without this, a full-width row two rows down
    // can outscore a half-width card immediately below.
    struct Cand { Pcsx2Card* card; long long primary; long long secondary; };
    QList<Cand> cands;
    long long minPrimary = std::numeric_limits<long long>::max();

    for (Pcsx2Card* card : findChildren<Pcsx2Card*>()) {
        if (card == current || !card->isVisible() || card->focusPolicy() == Qt::NoFocus) continue;
        const QRect r(pagePoint(card), card->size());
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
        if (!inDir || !perpOverlap) continue;

        // primary = signed distance along the nav axis (between rects, not centers,
        // so half-width cards on the same row tie). secondary = how far off-axis
        // the candidate's overlap-center is from the source's center.
        long long primary = 0, secondary = 0;
        if (vertical) {
            primary  = (key == Qt::Key_Down) ? qMax<int>(0, r.top() - mine.bottom())
                                             : qMax<int>(0, mine.top() - r.bottom());
            const int oL = qMax(mine.left(), r.left());
            const int oR = qMin(mine.right(), r.right());
            secondary = qAbs((oL + oR) / 2 - myCenter.x());
        } else {
            primary  = (key == Qt::Key_Right) ? qMax<int>(0, r.left() - mine.right())
                                              : qMax<int>(0, mine.left() - r.right());
            const int oT = qMax(mine.top(), r.top());
            const int oB = qMin(mine.bottom(), r.bottom());
            secondary = qAbs((oT + oB) / 2 - myCenter.y());
        }
        cands.push_back({card, primary, secondary});
        if (primary < minPrimary) minPrimary = primary;
    }
    if (cands.isEmpty()) return nullptr;

    // Within the closest row/column (primary ≈ minPrimary, with a small
    // tolerance for grid spacing differences), pick the most-aligned card.
    constexpr long long kRowTolerance = 8;
    Pcsx2Card* best = nullptr;
    long long bestSecondary = std::numeric_limits<long long>::max();
    for (const Cand& c : cands) {
        if (c.primary > minPrimary + kRowTolerance) continue;
        if (c.secondary < bestSecondary) { bestSecondary = c.secondary; best = c.card; }
    }
    return best;
}

void DuckStationConsolePage::refreshDependencies() {
    QHash<QString, bool> masterStates;
    for (auto* tog : findChildren<Pcsx2ToggleRow*>())
        masterStates.insert(tog->settingDef().key, tog->isChecked());

    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const SettingDef& d = slider->settingDef();
        if (d.dependsOn.isEmpty()) continue;
        const bool active = masterStates.value(d.dependsOn, true);
        slider->setProperty("dependencyActive", active);
        // Disable the inner QSlider so the user can't click or drag the track
        // while the master toggle is off — the row itself stays focusable so
        // arrow-key spatial nav still passes through it.
        if (auto* inner = slider->findChild<QSlider*>())
            inner->setEnabled(active);
        if (!active) {
            if (!slider->graphicsEffect()) {
                auto* eff = new QGraphicsOpacityEffect(slider);
                eff->setOpacity(0.4);
                slider->setGraphicsEffect(eff);
            }
        } else {
            slider->setGraphicsEffect(nullptr);
        }
    }
}
