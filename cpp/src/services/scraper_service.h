#pragma once

#include "core/database.h"
#include "core/scraper.h"
#include "services/scraper_credentials.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QAtomicInt>

class ScraperService : public QObject {
    Q_OBJECT

public:
    ScraperService(Database* db, QObject* parent = nullptr);

    /** Load credentials from disk and configure the scraper. */
    void loadCredentials();

    /** Validate user credentials against ScreenScraper API, save if valid. */
    bool validateAndSaveCredentials(const QString& ssId, const QString& ssPassword);

    /** Clear stored user credentials. */
    void signOut();

    /** True if user credentials are configured. */
    bool hasCredentials() const { return m_creds.hasUserCredentials(); }

    /** Access current credentials (read-only). */
    const ScraperCredentials& credentials() const { return m_creds; }

    /** Scrape a single game by ID (all media types). */
    struct ScrapeResult {
        bool success = false;
        QString message;
        int mediaDownloaded = 0;
    };
    ScrapeResult scrapeGame(int gameId);

    /** Batch scrape options. */
    struct ScrapeOptions {
        QStringList mediaTypes;
        QStringList systems;
        enum Filter { AllGames, UnscrapedOnly, FavoritesOnly };
        Filter gameFilter = AllGames;
    };

    /** Start a batch scrape on a background thread. */
    void startBatchScrape(const ScrapeOptions& options);

    /** Scrape a single game with progress signals (background thread). */
    void startSingleGameScrape(int gameId);

    /** Cancel a running batch scrape. */
    void cancelScrape();

signals:
    void statusMessage(const QString& msg);
    void scrapeProgress(int current, int total, const QVariantMap& gameData);
    void scrapeFinished(int succeeded, int failed, int skipped);
    void credentialsValidated(bool success, const QString& message);

private:
    void applyResultToDb(int gameId, const Scraper::ScrapeResult& result);

    Database* m_db;
    Scraper* m_scraper;
    ScraperCredentials m_creds;
    QAtomicInt m_cancelFlag;
};
