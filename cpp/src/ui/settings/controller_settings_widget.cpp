#include "controller_settings_widget.h"
#include "binding_widget_common.h"
#include "ui/app_controller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QVariantMap>

static const QString kTextDim       = "#666666";

static const QString kSliderStyle = QStringLiteral(
    "QSlider::groove:horizontal { background: #353558; height: 6px; border-radius: 3px; }"
    "QSlider::handle:horizontal { background: %1; width: 14px; height: 14px;"
    "  margin: -4px 0; border-radius: 7px; }"
    "QSlider::sub-page:horizontal { background: %1; border-radius: 3px; }"
).arg(kAccent);

ControllerSettingsWidget::ControllerSettingsWidget(AppController* appController,
                                                   const QString& emuId,
                                                   int port,
                                                   QWidget* parent)
    : QWidget(parent)
    , m_appController(appController)
    , m_emuId(emuId)
    , m_port(port)
{
    setStyleSheet(QString("background: %1;").arg(kBg));
    buildUI();
}

void ControllerSettingsWidget::buildUI() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QString(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical { background: %1; width: 8px; }"
        "QScrollBar::handle:vertical { background: #3a3a60; border-radius: 4px; min-height: 30px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(kBg));

    auto* content = new QWidget();
    content->setStyleSheet(QString("background: %1;").arg(kBg));
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(32, 24, 32, 24);
    layout->setSpacing(16);

    QVariantList settings = m_appController->controllerSettingsForPort(m_emuId, m_port);

    for (const auto& s : settings) {
        auto map = s.toMap();
        QString label = map["label"].toString();
        QString tooltip = map["tooltip"].toString();
        QString key = map["key"].toString();
        QString suffix = map["suffix"].toString();
        int type = map["type"].toInt();
        QString currentValue = map["currentValue"].toString();
        double minVal = map["minVal"].toDouble();
        double maxVal = map["maxVal"].toDouble();

        if (type == 4) { // Combo (SettingDef::Combo == 4)
            auto* row = new QHBoxLayout();
            row->setSpacing(16);

            auto* labelWidget = new QWidget();
            auto* labelLayout = new QVBoxLayout(labelWidget);
            labelLayout->setContentsMargins(0, 0, 0, 0);
            labelLayout->setSpacing(2);

            auto* nameLabel = new QLabel(label);
            nameLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(kTextPrimary));
            labelLayout->addWidget(nameLabel);

            if (!tooltip.isEmpty()) {
                auto* tipLabel = new QLabel(tooltip);
                tipLabel->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(kTextDim));
                tipLabel->setWordWrap(true);
                labelLayout->addWidget(tipLabel);
            }

            row->addWidget(labelWidget, 1);

            auto* combo = new QComboBox();
            combo->setFixedWidth(200);
            combo->setStyleSheet(QString(
                "QComboBox { background: #353558; color: %1; border: 1px solid %2;"
                "  border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
                "QComboBox::drop-down { border: none; }"
                "QComboBox QAbstractItemView { background: #1e1e3a; color: %1;"
                "  selection-background-color: %3; }"
            ).arg(kTextPrimary, kBoxBorder, kAccent));

            QVariantList opts = map["options"].toList();
            ComboRow cr;
            for (const auto& opt : opts) {
                auto optMap = opt.toMap();
                combo->addItem(optMap["label"].toString());
                cr.values.append(optMap["value"].toString());
            }
            cr.combo = combo;

            int idx = cr.values.indexOf(currentValue);
            if (idx >= 0) combo->setCurrentIndex(idx);

            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this, key](int index) { onComboChanged(key, index); });

            row->addWidget(combo);
            m_combos[key] = cr;
            layout->addLayout(row);

        } else { // Int/Float slider
            auto* nameLabel = new QLabel(label);
            nameLabel->setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(kTextPrimary));

            if (!tooltip.isEmpty()) {
                auto* tipLabel = new QLabel(tooltip);
                tipLabel->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;").arg(kTextDim));
                tipLabel->setWordWrap(true);
                layout->addWidget(nameLabel);
                layout->addWidget(tipLabel);
            } else {
                layout->addWidget(nameLabel);
            }

            auto* sliderRow = new QHBoxLayout();
            sliderRow->setSpacing(12);

            auto* slider = new QSlider(Qt::Horizontal);
            slider->setMinimum(static_cast<int>(minVal));
            slider->setMaximum(static_cast<int>(maxVal));
            slider->setValue(currentValue.toInt());
            slider->setStyleSheet(kSliderStyle);

            auto* valueLabel = new QLabel(currentValue + suffix);
            valueLabel->setFixedWidth(50);
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            valueLabel->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent;").arg(kTextSecondary));

            connect(slider, &QSlider::valueChanged, this, [this, key, valueLabel, suffix](int val) {
                valueLabel->setText(QString::number(val) + suffix);
                onSliderChanged(key, val);
            });

            sliderRow->addWidget(slider, 1);
            sliderRow->addWidget(valueLabel);

            m_sliders[key] = {slider, valueLabel, suffix};
            layout->addLayout(sliderRow);
        }
    }

    layout->addStretch();

    scrollArea->setWidget(content);
    outerLayout->addWidget(scrollArea);
}

void ControllerSettingsWidget::onSliderChanged(const QString& key, int value) {
    m_appController->saveControllerSettingForPort(m_emuId, m_port, key, QString::number(value));
}

void ControllerSettingsWidget::onComboChanged(const QString& key, int index) {
    auto it = m_combos.find(key);
    if (it != m_combos.end() && index >= 0 && index < it->values.size())
        m_appController->saveControllerSettingForPort(m_emuId, m_port, key, it->values[index]);
}
