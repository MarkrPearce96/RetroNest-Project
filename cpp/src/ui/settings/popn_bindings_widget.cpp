#include "popn_bindings_widget.h"
#include "binding_widget_common.h"

#include <QHBoxLayout>
#include <QPixmap>
#include <QVBoxLayout>

PopnBindingsWidget::PopnBindingsWidget(SdlInputManager* inputManager,
                                       AppController* appController,
                                       const QString& emuId,
                                       int port,
                                       Variant variant,
                                       QWidget* parent)
    : BindingsWidgetBase(inputManager, appController, emuId, port, parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // -- Controller image --
    m_imgLabel = new QLabel(this);
    QPixmap pix(":/AppUI/qml/AppUI/images/controllers/Popn.svg");
    if (!pix.isNull())
        m_imgLabel->setPixmap(pix.scaled(800, 320, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setStyleSheet("background: transparent; border: none;");
    mainLayout->addWidget(m_imgLabel, 0, Qt::AlignCenter);

    auto makeBindGroup = [&](const QString& label) -> QWidget* {
        auto* group = new QWidget(this);
        auto* vbox = new QVBoxLayout(group);
        vbox->setContentsMargins(4, 4, 4, 4);
        vbox->setSpacing(2);
        vbox->addWidget(makeLabel(group, label));
        auto* btn = new BindBtn(group);
        btn->setMinimumWidth(100);
        setupBtn(btn, label);
        vbox->addWidget(btn);
        return group;
    };

    // PCSX2 and DuckStation use different binding keys for the same physical
    // 9-button + Start/Select layout. Choose the right keys per variant.
    QStringList mainButtons;
    QStringList systemButtons;
    if (variant == Variant::DuckStation) {
        mainButtons = {"Left White", "Left Yellow", "Left Green", "Left Blue",
                       "Middle Red",
                       "Right Blue", "Right Green", "Right Yellow", "Right White"};
        systemButtons = {"Select", "Start"};
    } else {
        mainButtons = {"Yellow Left", "Yellow Right", "Blue Left", "Blue Right",
                       "White Left", "White Right", "Green Left", "Green Right",
                       "Red"};
        systemButtons = {"Start", "Select"};
    }

    auto* row1 = new QHBoxLayout();
    row1->setSpacing(8);
    row1->addStretch();
    for (const auto& label : mainButtons)
        row1->addWidget(makeBindGroup(label));
    row1->addStretch();
    mainLayout->addLayout(row1);

    auto* row2 = new QHBoxLayout();
    row2->setSpacing(12);
    row2->addStretch();
    for (const auto& label : systemButtons)
        row2->addWidget(makeBindGroup(label));
    row2->addStretch();
    mainLayout->addLayout(row2);

    mainLayout->addStretch();

    loadBindings();
}
