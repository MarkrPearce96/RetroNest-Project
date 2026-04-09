#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class WizardState : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString romsDir READ romsDir NOTIFY rootPathChanged)

public:
    explicit WizardState(QObject* parent = nullptr);

    QString rootPath() const;
    void setRootPath(const QString& path);

    QString romsDir() const;

    Q_INVOKABLE QString browseFolder(const QString& title);
    Q_INVOKABLE void openFolder(const QString& path);
    Q_INVOKABLE void ensureRomDirs(const QStringList& systemIds);
    Q_INVOKABLE void accept();

signals:
    void wizardAccepted();
    void rootPathChanged();

private:
    QString m_rootPath;
};
