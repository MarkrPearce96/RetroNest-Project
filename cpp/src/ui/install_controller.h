#pragma once

#include "emulator_list_model.h"
#include "core/manifest_loader.h"
#include <QObject>
#include <QString>

class InstallController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installing READ installing NOTIFY installingChanged)
    Q_PROPERTY(int installCurrent READ installCurrent NOTIFY installCurrentChanged)
    Q_PROPERTY(int installTotal READ installTotal NOTIFY installTotalChanged)
    Q_PROPERTY(QString installStatus READ installStatus NOTIFY installStatusChanged)
    Q_PROPERTY(bool installDone READ installDone NOTIFY installDoneChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)

public:
    explicit InstallController(EmulatorListModel* model, QObject* parent = nullptr);

    bool installing() const;
    int installCurrent() const;
    int installTotal() const;
    QString installStatus() const;
    bool installDone() const;
    double progress() const;

    Q_INVOKABLE void startInstall(const QString& rootPath);

signals:
    void installingChanged();
    void installCurrentChanged();
    void installTotalChanged();
    void installStatusChanged();
    void installDoneChanged();
    void progressChanged();

private:
    EmulatorListModel* m_model;
    bool m_installing = false;
    int m_installCurrent = 0;
    int m_installTotal = 0;
    QString m_installStatus;
    bool m_installDone = false;
};
