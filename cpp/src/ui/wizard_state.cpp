#include "wizard_state.h"
#include "core/paths.h"
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>

WizardState::WizardState(QObject* parent)
    : QObject(parent)
{
}

QString WizardState::rootPath() const { return m_rootPath; }

void WizardState::setRootPath(const QString& path) {
    if (m_rootPath != path) {
        m_rootPath = path;
        emit rootPathChanged();
    }
}

QString WizardState::romsDir() const {
    if (m_rootPath.isEmpty()) return {};
    return m_rootPath + "/roms";
}

QString WizardState::browseFolder(const QString& title) {
    return QFileDialog::getExistingDirectory(nullptr, title,
        QDir::homePath(), QFileDialog::ShowDirsOnly);
}

void WizardState::openFolder(const QString& path) {
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void WizardState::ensureRomDirs(const QStringList& systemIds) {
    if (m_rootPath.isEmpty()) return;
    Paths::setRoot(m_rootPath);
    Paths::ensureDirectories();
    Paths::ensureRomDirectories(systemIds);
}

void WizardState::accept() {
    emit wizardAccepted();
}
