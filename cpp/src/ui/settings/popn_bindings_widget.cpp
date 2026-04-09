#include "popn_bindings_widget.h"
#include "binding_widget_common.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>

// ─────────────────────────────────────────────────────────────

PopnBindingsWidget::PopnBindingsWidget(SdlInputManager* inputManager,
                                       AppController* appController,
                                       const QString& emuId,
                                       int port,
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

    // -- Row 1: 9 coloured buttons --
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(8);
    row1->addStretch();
    row1->addWidget(makeBindGroup("Yellow Left"));
    row1->addWidget(makeBindGroup("Yellow Right"));
    row1->addWidget(makeBindGroup("Blue Left"));
    row1->addWidget(makeBindGroup("Blue Right"));
    row1->addWidget(makeBindGroup("White Left"));
    row1->addWidget(makeBindGroup("White Right"));
    row1->addWidget(makeBindGroup("Green Left"));
    row1->addWidget(makeBindGroup("Green Right"));
    row1->addWidget(makeBindGroup("Red"));
    row1->addStretch();
    mainLayout->addLayout(row1);

    // -- Row 2: Start, Select --
    auto* row2 = new QHBoxLayout();
    row2->setSpacing(12);
    row2->addStretch();
    row2->addWidget(makeBindGroup("Start"));
    row2->addWidget(makeBindGroup("Select"));
    row2->addStretch();
    mainLayout->addLayout(row2);

    mainLayout->addStretch();

    loadBindings();
}
