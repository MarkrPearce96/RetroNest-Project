#include "wizard_state.h"
#include "core/paths.h"
#include "core/system_registry.h"
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFile>

WizardState::WizardState(QObject* parent)
    : QObject(parent)
{
}

QString WizardState::rootPath() const { return m_rootPath; }

void WizardState::setRootPath(const QString& path) {
    if (m_rootPath != path) {
        m_rootPath = path;
        emit rootPathChanged();
        // romsRoot()/biosRoot() derive from m_rootPath when unset, so their
        // QML-visible defaults must track the data folder live.
        emit romsRootChanged();
        emit biosRootChanged();
    }
}

QString WizardState::romsRoot() const {
    return m_romsRoot.isEmpty() ? (m_rootPath + "/roms") : m_romsRoot;
}

void WizardState::setRomsRoot(const QString& p) {
    const QString v = p.isEmpty() ? QString() : QDir::cleanPath(p);
    if (v == m_romsRoot) return;
    m_romsRoot = v;
    emit romsRootChanged();
}

QString WizardState::biosRoot() const {
    return m_biosRoot.isEmpty() ? (m_rootPath + "/bios") : m_biosRoot;
}

void WizardState::setBiosRoot(const QString& p) {
    const QString v = p.isEmpty() ? QString() : QDir::cleanPath(p);
    if (v == m_biosRoot) return;
    m_biosRoot = v;
    emit biosRootChanged();
}

QString WizardState::browseFolder(const QString& title) {
    return QFileDialog::getExistingDirectory(nullptr, title,
        QDir::homePath(), QFileDialog::ShowDirsOnly);
}

void WizardState::openFolder(const QString& path) {
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void WizardState::applyStorageLocations() {
    if (!QDir(m_rootPath).exists())
        m_createdRoots.insert(m_rootPath);

    Paths::setRoot(m_rootPath);
    Paths::setRomsRoot(m_romsRoot);
    Paths::setBiosRoot(m_biosRoot);
    QDir().mkpath(Paths::configDir());   // so RA/scraper cred saves work
}

void WizardState::discardIncompleteSetup() {
    // Called only when the wizard is closed WITHOUT finishing. Remove ONLY
    // folders the wizard created fresh — never a pre-existing folder/its data.
    for (const QString& r : m_createdRoots) {
        if (!r.isEmpty())
            QDir(r).removeRecursively();
    }
    // Belt-and-suspenders: if the current root PRE-existed (so it wasn't in
    // m_createdRoots), still remove the wizard's own credential files it may
    // have written into it — but nothing else in that folder.
    if (!m_rootPath.isEmpty()) {
        QFile::remove(m_rootPath + "/config/retroachievements.json");
        QFile::remove(m_rootPath + "/config/scraper.json");
    }
}

void WizardState::accept() {
    if (m_rootPath.isEmpty()) return;

    // Completing the wizard is what commits the chosen root — not a side
    // effect of any individual page (InstallController used to save it in
    // startInstall, so a flow that skipped the install page finished setup
    // without persisting the root and the wizard reappeared every launch).
    Paths::setRoot(m_rootPath);
    Paths::setRomsRoot(m_romsRoot);
    Paths::setBiosRoot(m_biosRoot);
    Paths::ensureDirectories();
    // Scaffold a ROM folder for every registered console, not just the
    // ones the user interacted with during the wizard.
    Paths::ensureRomDirectories(SystemRegistry::allSystemIds());

    Paths::saveRoot(m_rootPath);
    Paths::saveRomsRoot(m_romsRoot);
    Paths::saveBiosRoot(m_biosRoot);

    emit wizardAccepted();
}
