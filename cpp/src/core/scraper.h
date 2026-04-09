#pragma once

#include "database.h"
#include <QString>
#include <QObject>
#include <QMap>
#include <QStringList>
#include <QAtomicInt>
#include <functional>

class Scraper : public QObject {
    Q_OBJECT

public:
    explicit Scraper(QObject* parent = nullptr);

    void setCredentials(const QString& devId, const QString& devPassword,
                        const QString& softname);
    void setUserCredentials(const QString& userId, const QString& userPassword);

    struct ScrapeResult {
        bool success = false;
        QString error;

        QString title;
        QString description;
        QString developer;
        QString publisher;
        QString release_date;
        QString genres;
        double  rating = 0.0;
        QString players;

        QMap<QString, QString> mediaPaths;  // mediaType -> local file path

        // API quota (from ssuser in response)
        int requestsToday = 0;
        int maxRequestsPerDay = 0;
    };

    /** Emitted after metadata is parsed but before media downloads begin. */
    using MetadataCallback = std::function<void(const ScrapeResult&)>;

    ScrapeResult scrapeGame(const GameRecord& game,
                            const QString& mediaBaseDir,
                            const QStringList& mediaTypes,
                            QAtomicInt* cancelFlag = nullptr,
                            MetadataCallback onMetadata = nullptr);

    bool validateCredentials(const QString& userId, const QString& userPassword);

    static QStringList allMediaTypes();

signals:
    void progress(int current, int total, const QString& gameName);

private:
    static int systemToScreenScraperId(const QString& system);
    static QStringList screenScraperMediaTypes(const QString& mediaType);

    QByteArray httpGet(const QString& url, QAtomicInt* cancelFlag = nullptr);

    struct MediaDownload {
        QString mediaType;
        QString url;
        QString destPath;
    };
    QMap<QString, QString> downloadMediaParallel(const QVector<MediaDownload>& downloads,
                                                  QAtomicInt* cancelFlag);

    QString m_devId;
    QString m_devPassword;
    QString m_softname;
    QString m_userId;
    QString m_userPassword;
};
