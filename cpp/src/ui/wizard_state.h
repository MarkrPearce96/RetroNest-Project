#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class WizardState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString romsDir READ romsDir NOTIFY rootPathChanged)
    Q_PROPERTY(QString romsRoot READ romsRoot WRITE setRomsRoot NOTIFY romsRootChanged)
    Q_PROPERTY(QString biosRoot READ biosRoot WRITE setBiosRoot NOTIFY biosRootChanged)

public:
    explicit WizardState(QObject* parent = nullptr);

    QString rootPath() const;
    void setRootPath(const QString& path);

    QString romsDir() const;

    QString romsRoot() const;
    void setRomsRoot(const QString& p);
    QString biosRoot() const;
    void setBiosRoot(const QString& p);

    Q_INVOKABLE QString browseFolder(const QString& title);
    Q_INVOKABLE void openFolder(const QString& path);
    Q_INVOKABLE void ensureRomDirs(const QStringList& systemIds);
    /** Applies the chosen storage roots early (before accept()) so later
     *  wizard steps that persist to {root}/config/ (RA login, scraper
     *  credentials) have somewhere to write. Idempotent — accept() still
     *  finalizes everything at the end of the wizard. */
    Q_INVOKABLE void applyStorageLocations();
    Q_INVOKABLE void accept();

signals:
    void wizardAccepted();
    void rootPathChanged();
    void romsRootChanged();
    void biosRootChanged();

private:
    QString m_rootPath;
    QString m_romsRoot;   // empty ⇒ {rootPath}/roms
    QString m_biosRoot;   // empty ⇒ {rootPath}/bios
};
