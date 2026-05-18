#include "pcsx2_patches_page.h"

#include "core/paths.h"
#include "ui/app_controller.h"

#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

Pcsx2PatchesPage::Pcsx2PatchesPage(AppController* app, QWidget* parent)
    : QWidget(parent), m_app(app) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* heading = new QLabel("PCSX2 Patches", this);
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    auto* blurb = new QLabel(
        "Widescreen, no-interlacing, and per-game game-fix patches are "
        "maintained by the PCSX2 community at github.com/PCSX2/pcsx2_patches. "
        "RetroNest auto-checks for updates on launch (every 90 days). Use "
        "the button below to force a refresh.", this);
    blurb->setWordWrap(true);
    layout->addWidget(blurb);

    m_status = new QLabel(this);
    m_status->setStyleSheet("color: #888;");
    layout->addWidget(m_status);
    refreshStatusLabel();

    m_button = new QPushButton("Refresh PCSX2 Patches", this);
    QObject::connect(m_button, &QPushButton::clicked, this, [this]() {
        m_button->setEnabled(false);
        m_button->setText("Refreshing…");
        m_app->refreshPcsx2Patches();
        // Re-enable + restore label once the fetch completes. The toast
        // is the primary user-visible feedback; this timer is just for
        // the in-page button + label state. 8 seconds is generous for a
        // few-MB download.
        QTimer::singleShot(8000, this, [this]() {
            m_button->setEnabled(true);
            m_button->setText("Refresh PCSX2 Patches");
            refreshStatusLabel();
        });
    });
    layout->addWidget(m_button);
    layout->addStretch(1);
}

void Pcsx2PatchesPage::refreshStatusLabel() {
    const QString zipPath = Paths::pcsx2ResourcesDir() + "/patches.zip";

    // Per spec non-goal: no detailed version readout in the settings UI.
    // Just installed / not-installed status so the refresh button has
    // a tiny bit of context.
    m_status->setText(QFile::exists(zipPath) ? "Status: installed"
                                              : "Status: not installed");
}
