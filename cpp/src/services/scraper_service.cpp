#include "scraper_service.h"
#include "core/paths.h"

#include <QFileInfo>
#include <QThread>
#include <QtConcurrent>

namespace {
// Populate the QML-facing progress map with fields from a successful scrape.
// Used by both batch and single-game progress emissions.
void fillProgressMapFromResult(QVariantMap& gameData, const Scraper::ScrapeResult& result) {
    gameData["scrapedTitle"] = result.title;
    gameData["description"] = result.description;
    gameData["developer"] = result.developer;
    gameData["publisher"] = result.publisher;
    gameData["releaseDate"] = result.release_date;
    gameData["genres"] = result.genres;
    gameData["rating"] = result.rating;
    gameData["players"] = result.players;
    gameData["coverPath"] = result.mediaPaths.value("covers");
    gameData["screenshotPath"] = result.mediaPaths.value("screenshots");
    gameData["requestsToday"] = result.requestsToday;
    gameData["maxRequests"] = result.maxRequestsPerDay;
}
} // namespace

ScraperService::ScraperService(Database* db, QObject* parent)
    : QObject(parent), m_db(db), m_scraper(new Scraper(this))
{
    m_cancelFlag.storeRelaxed(0);
}

void ScraperService::loadCredentials() {
    m_creds.load();
    m_scraper->setCredentials(m_creds.devId, m_creds.devPassword, m_creds.softname);
    if (m_creds.hasUserCredentials())
        m_scraper->setUserCredentials(m_creds.ssId, m_creds.ssPassword);
}

void ScraperService::validateAndSaveCredentials(const QString& ssId, const QString& ssPassword) {
    // Ensure dev credentials are set on the scraper before the worker starts
    // reading them.
    m_scraper->setCredentials(m_creds.devId, m_creds.devPassword, m_creds.softname);

    // Run the HTTP validation on a worker thread. Scraper::httpGet spins a
    // nested QEventLoop, which must never happen on the GUI thread while a
    // QML signal handler is on the stack — DeferredDelete processing there
    // can destroy the in-progress handler's QObject and trip QQmlData's
    // fatal guard.
    (void)QtConcurrent::run([this, ssId, ssPassword]() {
        bool valid = m_scraper->validateCredentials(ssId, ssPassword);

        // Mutate credentials state and emit the result on the main thread.
        QMetaObject::invokeMethod(this, [this, valid, ssId, ssPassword]() {
            if (valid) {
                m_creds.ssId = ssId;
                m_creds.ssPassword = ssPassword;
                m_creds.save();
                m_scraper->setUserCredentials(ssId, ssPassword);
                emit credentialsValidated(true, "Credentials validated successfully.");
            } else {
                emit credentialsValidated(false,
                    "Invalid credentials. Please check your username and password.");
            }
        }, Qt::QueuedConnection);
    });
}

void ScraperService::signOut() {
    m_creds.clearUser();
    m_scraper->setUserCredentials("", "");
    emit statusMessage("Signed out of ScreenScraper.");
}

void ScraperService::applyResultToDb(int gameId, const Scraper::ScrapeResult& result) {
    GameRecord metadata;
    metadata.title        = result.title;
    metadata.description  = result.description;
    metadata.developer    = result.developer;
    metadata.publisher    = result.publisher;
    metadata.release_date = result.release_date;
    metadata.genres       = result.genres;
    metadata.rating       = result.rating;
    metadata.players      = result.players;

    // Map media paths to GameRecord fields
    metadata.cover_path        = result.mediaPaths.value("covers");
    metadata.screenshot_path   = result.mediaPaths.value("screenshots");
    metadata.titlescreen_path  = result.mediaPaths.value("titlescreens");
    metadata.marquee_path      = result.mediaPaths.value("marquees");
    metadata.fanart_path       = result.mediaPaths.value("fanart");
    metadata.box3d_path        = result.mediaPaths.value("3dboxes");
    metadata.backcover_path    = result.mediaPaths.value("backcovers");
    metadata.miximage_path     = result.mediaPaths.value("miximages");
    metadata.physicalmedia_path = result.mediaPaths.value("physicalmedia");
    metadata.manual_path       = result.mediaPaths.value("manuals");
    metadata.video_path        = result.mediaPaths.value("videos");

    if (!m_db->updateGameMetadata(gameId, metadata))
        qWarning() << "ScraperService: failed to write metadata for game ID" << gameId;
}

void ScraperService::startBatchScrape(const ScrapeOptions& options) {
    m_cancelFlag.storeRelaxed(0);

    // Gather game list on the main thread (DB connection is thread-bound)
    QVector<GameRecord> allGames;
    for (const auto& system : options.systems) {
        auto games = m_db->gamesBySystem(system);
        allGames.append(games);
    }

    // Apply game filter on main thread
    QVector<GameRecord> toScrape;
    for (const auto& g : allGames) {
        switch (options.gameFilter) {
        case ScrapeOptions::UnscrapedOnly:
            if (g.cover_path.isEmpty() || !QFileInfo::exists(g.cover_path))
                toScrape.append(g);
            break;
        case ScrapeOptions::FavoritesOnly:
            if (g.favorite)
                toScrape.append(g);
            break;
        case ScrapeOptions::AllGames:
        default:
            toScrape.append(g);
            break;
        }
    }

    if (toScrape.isEmpty()) {
        emit scrapeFinished(0, 0, 0);
        return;
    }

    (void)QtConcurrent::run([this, toScrape, options]() {
        int succeeded = 0, failed = 0, skipped = 0;
        QString mediaBaseDir = Paths::mediaDir();

        // Accumulate successful scrape results for batched DB write
        QVector<QPair<int, Scraper::ScrapeResult>> pendingWrites;

        for (int i = 0; i < toScrape.size(); i++) {
            if (m_cancelFlag.loadRelaxed()) {
                skipped = toScrape.size() - i;
                break;
            }

            const auto& game = toScrape[i];

            // Emit "scraping" state with game name
            QVariantMap startData;
            startData["gameName"] = game.title;
            startData["status"] = "scraping";
            emit scrapeProgress(i + 1, toScrape.size(), startData);

            auto result = m_scraper->scrapeGame(game, mediaBaseDir, options.mediaTypes, &m_cancelFlag);

            QVariantMap gameData;
            gameData["gameName"] = game.title;

            if (result.success) {
                pendingWrites.append({game.id, result});

                const int count = result.mediaPaths.size();
                const int requested = options.mediaTypes.size();
                if (count < requested) {
                    gameData["status"] = QString("%1 media (%2 not available)").arg(count).arg(requested - count);
                } else {
                    gameData["status"] = QString("%1 media downloaded").arg(count);
                }

                // Full metadata + media paths for final update
                fillProgressMapFromResult(gameData, result);

                succeeded++;
            } else {
                gameData["status"] = "failed: " + result.error;
                failed++;
            }

            emit scrapeProgress(i + 1, toScrape.size(), gameData);

            // Rate limiting (cancellation-aware)
            if (i < toScrape.size() - 1) {
                for (int ms = 0; ms < 1200 && !m_cancelFlag.loadRelaxed(); ms += 100)
                    QThread::msleep(100);
            }
        }

        // Bounce ALL DB writes + scrapeFinished to the main thread in one call.
        // This guarantees every write completes before gamesChanged() fires.
        QMetaObject::invokeMethod(this, [this, pendingWrites, succeeded, failed, skipped]() {
            for (const auto& pending : pendingWrites) {
                applyResultToDb(pending.first, pending.second);
            }
            emit scrapeFinished(succeeded, failed, skipped);
        }, Qt::QueuedConnection);
    });
}

void ScraperService::cancelScrape() {
    m_cancelFlag.storeRelaxed(1);
}

void ScraperService::startSingleGameScrape(int gameId) {
    GameRecord game = m_db->gameById(gameId);
    if (game.id == 0) {
        emit scrapeFinished(0, 1, 0);
        return;
    }

    m_cancelFlag.storeRelaxed(0);

    (void)QtConcurrent::run([this, game]() {
        QString mediaBaseDir = Paths::mediaDir();

        QVariantMap startData;
        startData["gameName"] = game.title;
        startData["status"] = "scraping";
        emit scrapeProgress(1, 1, startData);

        auto result = m_scraper->scrapeGame(game, mediaBaseDir, Scraper::allMediaTypes(), &m_cancelFlag);

        QVariantMap gameData;
        gameData["gameName"] = game.title;

        int succeeded = 0, failed = 0;
        if (result.success) {
            gameData["status"] = QString("%1 media downloaded").arg(result.mediaPaths.size());
            fillProgressMapFromResult(gameData, result);
            succeeded = 1;
        } else {
            gameData["status"] = "failed: " + result.error;
            failed = 1;
        }

        emit scrapeProgress(1, 1, gameData);

        QMetaObject::invokeMethod(this, [this, game, result, succeeded, failed]() {
            if (result.success)
                applyResultToDb(game.id, result);
            emit scrapeFinished(succeeded, failed, 0);
        }, Qt::QueuedConnection);
    });
}
