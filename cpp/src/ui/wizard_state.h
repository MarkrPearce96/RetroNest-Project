#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class WizardState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString romsRoot READ romsRoot WRITE setRomsRoot NOTIFY romsRootChanged)
    Q_PROPERTY(QString biosRoot READ biosRoot WRITE setBiosRoot NOTIFY biosRootChanged)

public:
    explicit WizardState(QObject* parent = nullptr);

    QString rootPath() const;
    void setRootPath(const QString& path);

    QString romsRoot() const;
    void setRomsRoot(const QString& p);
    QString biosRoot() const;
    void setBiosRoot(const QString& p);

    Q_INVOKABLE QString browseFolder(const QString& title);
    Q_INVOKABLE void openFolder(const QString& path);
    /** Applies the chosen storage roots early (before accept()) so later
     *  wizard steps that persist to {root}/config/ (RA login, scraper
     *  credentials) have somewhere to write. Idempotent — accept() still
     *  finalizes everything at the end of the wizard. */
    Q_INVOKABLE void applyStorageLocations();
    Q_INVOKABLE void accept();

    /** Called only when the wizard is closed WITHOUT finishing. Removes ONLY
     *  folders the wizard created fresh this run (never a pre-existing
     *  folder/its data), plus any wizard-written credential files it may
     *  have dropped into a pre-existing root. Not Q_INVOKABLE — called from
     *  C++ (main.cpp) only. */
    void discardIncompleteSetup();

signals:
    void wizardAccepted();
    void rootPathChanged();
    void romsRootChanged();
    void biosRootChanged();

private:
    QString m_rootPath;
    QString m_romsRoot;   // empty ⇒ {rootPath}/roms
    QString m_biosRoot;   // empty ⇒ {rootPath}/bios
    QSet<QString> m_createdRoots;
};
