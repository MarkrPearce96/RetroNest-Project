#include "guitar_bindings_widget.h"
#include "binding_widget_common.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPixmap>

// ─────────────────────────────────────────────────────────────

GuitarBindingsWidget::GuitarBindingsWidget(SdlInputManager* inputManager,
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
    QPixmap pix(":/AppUI/qml/AppUI/images/controllers/Guitar.svg");
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
        btn->setMinimumWidth(120);
        setupBtn(btn, label);
        vbox->addWidget(btn);
        return group;
    };

    // -- Row 1: Start, Select --
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(12);
    row1->addStretch();
    row1->addWidget(makeBindGroup("Start"));
    row1->addWidget(makeBindGroup("Select"));
    row1->addStretch();
    mainLayout->addLayout(row1);

    // -- Row 2: Fret buttons --
    auto* row2 = new QHBoxLayout();
    row2->setSpacing(12);
    row2->addStretch();
    row2->addWidget(makeBindGroup("Green"));
    row2->addWidget(makeBindGroup("Red"));
    row2->addWidget(makeBindGroup("Yellow"));
    row2->addWidget(makeBindGroup("Blue"));
    row2->addWidget(makeBindGroup("Orange"));
    row2->addStretch();
    mainLayout->addLayout(row2);

    // -- Row 3: Strum, Whammy, Tilt --
    auto* row3 = new QHBoxLayout();
    row3->setSpacing(12);
    row3->addStretch();
    row3->addWidget(makeBindGroup("Strum Up"));
    row3->addWidget(makeBindGroup("Strum Down"));
    row3->addWidget(makeBindGroup("Whammy"));
    row3->addWidget(makeBindGroup("Tilt"));
    row3->addStretch();
    mainLayout->addLayout(row3);

    mainLayout->addStretch();

    loadBindings();
}
