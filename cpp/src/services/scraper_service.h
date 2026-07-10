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

    /**
     * Validate user credentials against the ScreenScraper API and save on
     * success. Runs on a worker thread; the result is delivered via the
     * credentialsValidated() signal on the main thread.
     */
    Q_INVOKABLE void validateAndSaveCredentials(const QString& ssId, const QString& ssPassword);

    /** Clear stored user credentials. */
    void signOut();

    /** True if user credentials are configured. */
    bool hasCredentials() const { return m_creds.hasUserCredentials(); }

    /** Access current credentials (read-only). */
    const ScraperCredentials& credentials() const { return m_creds; }

    /** Batch scrape options. */
    struct ScrapeOptions {
        QStringList mediaTypes;
        QStringList systems;
        enum Filter { AllGames, UnscrapedOnly, FavoritesOnly };
        Filter gameFilter = AllGames;

        /** Parse the QML-facing filter string ("unscraped" / "favorites";
         *  anything else means all games). */
        static Filter filterFromString(const QString& s);
    };

    /**
     * The single scrape-set membership predicate. startBatchScrape and any
     * pre-count shown to the user (AppController::scrapeGameCount) must both
     * go through this so the confirmation count can't drift from the set
     * actually scraped.
     */
    static bool matchesFilter(const GameRecord& g, ScrapeOptions::Filter f);

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
