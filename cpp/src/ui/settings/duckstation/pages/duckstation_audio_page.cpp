#include "duckstation_audio_page.h"
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
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVariantMap>
#include <QLabel>
#include <QFrame>
#include <QSlider>
#include <QGraphicsOpacityEffect>

DuckStationAudioPage::DuckStationAudioPage(DuckStationSettingsDialog* dialog)
    : QWidget(dialog), m_dialog(dialog) {
    DuckStationAdapter adapter;
    for (const auto& d : adapter.settingsSchema())
        if (d.category == "Audio") m_schema.append(d);
    buildUi();
    loadValues();
}

const SettingDef* DuckStationAudioPage::findDef(const QString& key) const {
    for (const auto& d : m_schema) if (d.key == key) return &d;
    return nullptr;
}

void DuckStationAudioPage::buildUi() {
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationAudioPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::focused, this, &DuckStationAudioPage::settingFocused);
        connect(row,  &Pcsx2ComboRow::valueChanged, this, [this, key](const QString& v){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, v);
            // Stretch mode contributes the SoundTouch sequence length term to
            // the Maximum Latency formula, so refresh on change.
            if (key == "StretchMode") refreshLatencyLabel();
        });
        v->addWidget(row);
        return card;
    };
    auto makeSliderCard = [this](const QString& key) -> Pcsx2Card* {
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationAudioPage::settingFocused);
        connect(row, &Pcsx2SliderRow::focused, this, &DuckStationAudioPage::settingFocused);
        connect(row, &Pcsx2SliderRow::valueChanged, this, [this, key](int val){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, QString::number(val));
            if (key == "BufferMS" || key == "OutputLatencyMS"
             || key == "StretchSequenceLengthMS")
                refreshLatencyLabel();
        });
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
        connect(card, &Pcsx2Card::focused, this, &DuckStationAudioPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::focused, this, &DuckStationAudioPage::settingFocused);
        connect(row, &Pcsx2ToggleRow::toggled, this, [this, key](bool on){
            if (const SettingDef* d2 = findDef(key)) saveValue(d2->section, d2->key, on ? "true" : "false");
            // Buffer/latency configuration affects the Maximum Latency label.
            if (key == "OutputLatencyMinimal") refreshLatencyLabel();
        });
        v->addWidget(row);
        return card;
    };

    // Configuration — mirrors DuckStation's audio settings widget:
    //   • Backend + Driver paired in one row
    //   • Output Device on its own row
    //   • Stretch Mode on its own row
    //   • Buffer Size slider on its own row
    //   • Output Latency slider + Minimal toggle paired in one row
    //   • Maximum Latency info label below
    root->addWidget(new Pcsx2SectionHeader("Configuration", this));

    auto* backendRow = new QHBoxLayout();
    backendRow->setSpacing(10);
    if (auto* c = makeComboCard("Backend")) backendRow->addWidget(c, 1);
    if (auto* c = makeComboCard("Driver"))  backendRow->addWidget(c, 1);
    root->addLayout(backendRow);

    if (auto* c = makeComboCard("OutputDevice"))      root->addWidget(c);
    if (auto* c = makeComboCard("StretchMode"))       root->addWidget(c);
    if (auto* c = makeSliderCard("BufferMS"))         root->addWidget(c);

    auto* latencyRow = new QHBoxLayout();
    latencyRow->setSpacing(10);
    if (auto* slCard = makeSliderCard("OutputLatencyMS")) {
        slCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        latencyRow->addWidget(slCard, 1);
    }
    if (auto* minCard = makeToggleCard("OutputLatencyMinimal")) {
        minCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        latencyRow->addWidget(minCard, 0);
    }
    root->addLayout(latencyRow);

    // Maximum Latency info label — recomputed whenever buffer / output latency /
    // stretch mode / minimal-latency toggle changes.
    m_latencyLabel = new QLabel(content);
    m_latencyLabel->setStyleSheet("color:#9a9690;font-size:12px;padding:4px 4px 4px 14px;");
    m_latencyLabel->setWordWrap(true);
    root->addWidget(m_latencyLabel);

    // Volume Controls
    root->addWidget(new Pcsx2SectionHeader("Volume Controls", this));
    if (auto* volCard = makeSliderCard("OutputVolume")) root->addWidget(volCard);

    auto* volRow = new QHBoxLayout();
    volRow->setSpacing(10);
    if (auto* ffCard = makeSliderCard("FastForwardVolume")) {
        ffCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        volRow->addWidget(ffCard, 1);
    }
    if (auto* muteCard = makeToggleCard("OutputMuted")) {
        muteCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        volRow->addWidget(muteCard, 0);
    }
    root->addLayout(volRow);

    if (auto* c = makeToggleCard("MuteCDAudio")) root->addWidget(c);

    // Time Stretching
    root->addWidget(new Pcsx2SectionHeader("Time Stretching", this));
    if (auto* c = makeSliderCard("StretchSequenceLengthMS")) root->addWidget(c);
    if (auto* c = makeSliderCard("StretchSeekWindowMS"))     root->addWidget(c);
    if (auto* c = makeSliderCard("StretchOverlapMS"))        root->addWidget(c);

    auto* stretchRow = new QHBoxLayout();
    stretchRow->setSpacing(10);
    if (auto* qsCard = makeToggleCard("StretchUseQuickSeek")) {
        qsCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        stretchRow->addWidget(qsCard, 1);
    }
    if (auto* aaCard = makeToggleCard("StretchUseAAFilter")) {
        aaCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        stretchRow->addWidget(aaCard, 1);
    }
    root->addLayout(stretchRow);

    root->addStretch();
}

void DuckStationAudioPage::loadValues() {
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
        QString cur = app->settingValue(emuId, d.section, d.key);
        const QString v = cur.isEmpty() ? d.defaultValue : cur;
        slider->setValue(v.toInt());
    }
    refreshLatencyLabel();
}

void DuckStationAudioPage::saveValue(const QString& section, const QString& key, const QString& value) {
    QVariantMap m;
    m[section + "/" + key] = value;
    m_dialog->appController()->saveSettings(m_dialog->emuId(), m);
}

void DuckStationAudioPage::refreshLatencyLabel() {
    if (!m_latencyLabel) return;
    int bufferMs = 50, outputLatencyMs = 20, stretchSeqMs = 30;
    bool minimal = false;
    QString stretchMode = QStringLiteral("TimeStretch");

    Pcsx2SliderRow* outputSlider = nullptr;
    for (auto* slider : findChildren<Pcsx2SliderRow*>()) {
        const QString& k = slider->settingDef().key;
        if      (k == "BufferMS")                bufferMs        = slider->value();
        else if (k == "OutputLatencyMS")       { outputLatencyMs = slider->value(); outputSlider = slider; }
        else if (k == "StretchSequenceLengthMS") stretchSeqMs    = slider->value();
    }
    for (auto* tog : findChildren<Pcsx2ToggleRow*>()) {
        if (tog->settingDef().key == "OutputLatencyMinimal") minimal = tog->isChecked();
    }
    for (auto* combo : findChildren<Pcsx2ComboRow*>()) {
        if (combo->settingDef().key == "StretchMode") stretchMode = combo->value();
    }

    // Output Latency slider: when Minimal is on, swap the value label for
    // "N/A" and disable the inner QSlider — same behaviour as DuckStation
    // upstream, which sets the label to N/A and disables the slider.
    if (outputSlider) {
        if (minimal) {
            outputSlider->setValueFormatter([](int){ return QStringLiteral("N/A"); });
        } else {
            outputSlider->setValueFormatter({});  // revert to default "<n><suffix>"
        }
        if (auto* inner = outputSlider->findChild<QSlider*>())
            inner->setEnabled(!minimal);
        if (minimal) {
            if (!outputSlider->graphicsEffect()) {
                auto* eff = new QGraphicsOpacityEffect(outputSlider);
                eff->setOpacity(0.4);
                outputSlider->setGraphicsEffect(eff);
            }
        } else {
            outputSlider->setGraphicsEffect(nullptr);
        }
    }

    // Match upstream: stretch term only contributes when TimeStretch is active;
    // output term is dropped when "Minimal" is on (we don't have access to the
    // device's true minimum latency, so we report 0 there).
    const int stretchTerm = (stretchMode == "TimeStretch") ? stretchSeqMs : 0;
    const int outputTerm  = minimal ? 0 : outputLatencyMs;
    const int total       = stretchTerm + bufferMs + outputTerm;
    m_latencyLabel->setText(QStringLiteral(
        "Maximum Latency: %1 ms (%2 ms stretch + %3 ms buffer + %4 ms output)")
        .arg(total).arg(stretchTerm).arg(bufferMs).arg(outputTerm));
}
