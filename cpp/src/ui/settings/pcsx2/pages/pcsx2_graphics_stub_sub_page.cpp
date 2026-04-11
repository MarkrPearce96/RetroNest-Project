#include "pcsx2_graphics_stub_sub_page.h"
#include "../pcsx2_theme.h"
#include "ui/settings/emulator_settings_page.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

Pcsx2GraphicsStubSubPage::Pcsx2GraphicsStubSubPage(AppController* app,
                                                   const QString& emuId,
                                                   const QString& subTabName,
                                                   QWidget* parent)
    : QWidget(parent), m_app(app), m_emuId(emuId), m_subTabName(subTabName) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 40, 24, 40);
    root->setSpacing(14);
    root->addStretch();

    auto* title = new QLabel(QStringLiteral("%1 — Coming in a later update").arg(m_subTabName), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color:#f2efe8;font-size:18px;font-weight:600;");

    auto* sub = new QLabel(
        "This sub-page is still being redesigned. Use the legacy settings\n"
        "dialog in the meantime — all settings remain fully functional.",
        this);
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("color:#d0ccc4;font-size:13px;");

    auto* btn = new QPushButton("Open in legacy settings", this);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(
        "QPushButton {"
        "  background:#4a4642; color:#f2efe8;"
        "  border:1px solid #706c66; border-radius:4px;"
        "  padding:8px 18px; font-size:13px;"
        "}"
        "QPushButton:focus { border-color:#f59e0b; }"
        "QPushButton:hover { background:#585450; }");
    connect(btn, &QPushButton::clicked, this, &Pcsx2GraphicsStubSubPage::openLegacyDialog);

    root->addWidget(title);
    root->addWidget(sub);
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(btn);
    btnRow->addStretch();
    root->addLayout(btnRow);
    root->addStretch();
}

void Pcsx2GraphicsStubSubPage::openLegacyDialog() {
    auto* legacy = new EmulatorSettingsPage(m_app, m_emuId);
    legacy->setAttribute(Qt::WA_DeleteOnClose);
    legacy->setWindowModality(Qt::ApplicationModal);
    legacy->show();
}
