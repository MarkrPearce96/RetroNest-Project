#include "scraper.h"
#include "system_registry.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QUrlQuery>
#include <QFile>
#include <QThread>
#include <QTimer>

static const QString API_BASE = QStringLiteral("https://www.screenscraper.fr/api2/");

Scraper::Scraper(QObject* parent) : QObject(parent) {
}

void Scraper::setCredentials(const QString& devId, const QString& devPassword,
                              const QString& softname) {
    m_devId = devId;
    m_devPassword = devPassword;
    m_softname = softname;
}

void Scraper::setUserCredentials(const QString& userId, const QString& userPassword) {
    m_userId = userId;
    m_userPassword = userPassword;
}

// Returns our 11 media type folder names
QStringList Scraper::allMediaTypes() {
    return {
        "covers",
        "screenshots",
        "titlescreens",
        "3dboxes",
        "backcovers",
        "fanart",
        "marquees",
        "miximages",
        "physicalmedia",
        "manuals",
        "videos",
    };
}

// Maps our folder names to ScreenScraper API type strings
QStringList Scraper::screenScraperMediaTypes(const QString& mediaType) {
    static const QMap<QString, QStringList> map = {
        {"covers",        {"box-2D"}},
        {"screenshots",   {"ss"}},
        {"titlescreens",  {"sstitle"}},
        {"3dboxes",       {"box-3D"}},
        {"backcovers",    {"box-2D-back"}},
        {"fanart",        {"fanart"}},
        {"marquees",      {"screenmarquee"}},
        {"miximages",     {"mixrbv1", "mixrbv2"}},
        {"physicalmedia", {"support-2D"}},
        {"manuals",       {"manuel"}},
        {"videos",        {"video", "video-normalized"}},
    };
    return map.value(mediaType);
}

int Scraper::systemToScreenScraperId(const QString& system) {
    // Packet 7 stage 3: IDs come from manifests/systems.json.
    // (Reference list: https://www.screenscraper.fr/webapi2.php?action=systemesListe)
    return SystemRegistry::screenScraperId(system);
}

Scraper::ScrapeResult Scraper::scrapeGame(const GameRecord& game,
                                           const QString& mediaBaseDir,
                                           const QStringList& mediaTypes,
                                           QAtomicInt* cancelFlag,
                                           MetadataCallback onMetadata) {
    ScrapeResult result;

    int systemId = systemToScreenScraperId(game.system);
    if (systemId < 0) {
        result.error = "Unknown system: " + game.system;
        return result;
    }

    // Build API URL
    QString romFileName = QFileInfo(game.rom_path).fileName();

    QUrlQuery query;
    query.addQueryItem("devid",       m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname",    m_softname);
    query.addQueryItem("ssid",        m_userId);
    query.addQueryItem("sspassword",  m_userPassword);
    query.addQueryItem("output",      "json");
    query.addQueryItem("systemeid",   QString::number(systemId));
    query.addQueryItem("romnom",      romFileName);

    QString url = API_BASE + "jeuInfos.php?" + query.toString(QUrl::FullyEncoded);

    qInfo() << "[Scraper] Looking up:" << romFileName;

    QByteArray data = httpGet(url, cancelFlag);
    if (cancelFlag && cancelFlag->loadRelaxed()) {
        result.error = "Cancelled";
        return result;
    }
    if (data.isEmpty()) {
        result.error = "No response from ScreenScraper";
        return result;
    }

    // Parse response
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull()) {
        result.error = "JSON parse error: " + parseErr.errorString();
        return result;
    }

    QJsonObject root     = doc.object();
    QJsonObject response = root["response"].toObject();
    QJsonObject jeu      = response["jeu"].toObject();

    // Extract API quota from ssuser
    QJsonObject ssuser = response["ssuser"].toObject();
    if (!ssuser.isEmpty()) {
        result.requestsToday = ssuser["requeststoday"].toString().toInt();
        result.maxRequestsPerDay = ssuser["maxrequestsperday"].toString().toInt();
    }

    if (jeu.isEmpty()) {
        result.error = "Game not found on ScreenScraper";
        return result;
    }

    // --- Metadata extraction ---

    // Title (prefer us/wor, fall back to first)
    QJsonArray noms = jeu["noms"].toArray();
    for (const auto& nom : noms) {
        QJsonObject n = nom.toObject();
        QString region = n["region"].toString();
        if (region == "us" || region == "wor") {
            result.title = n["text"].toString();
            break;
        }
    }
    if (result.title.isEmpty() && !noms.isEmpty())
        result.title = noms[0].toObject()["text"].toString();

    // Description (prefer English)
    QJsonArray synopsis = jeu["synopsis"].toArray();
    for (const auto& s : synopsis) {
        QJsonObject obj = s.toObject();
        if (obj["langue"].toString() == "en") {
            result.description = obj["text"].toString();
            break;
        }
    }
    if (result.description.isEmpty() && !synopsis.isEmpty())
        result.description = synopsis[0].toObject()["text"].toString();

    // Developer
    QJsonObject developpeur = jeu["developpeur"].toObject();
    if (!developpeur.isEmpty())
        result.developer = developpeur["text"].toString();

    // Publisher
    QJsonObject editeur = jeu["editeur"].toObject();
    if (!editeur.isEmpty())
        result.publisher = editeur["text"].toString();

    // Release date (first available)
    QJsonArray dates = jeu["dates"].toArray();
    if (!dates.isEmpty())
        result.release_date = dates[0].toObject()["text"].toString();

    // Genres (comma-separated, prefer English)
    QJsonArray genres = jeu["genres"].toArray();
    QStringList genreList;
    for (const auto& genre : genres) {
        QJsonArray noms_genre = genre.toObject()["noms"].toArray();
        bool found = false;
        for (const auto& nom : noms_genre) {
            QJsonObject n = nom.toObject();
            if (n["langue"].toString() == "en") {
                genreList.append(n["text"].toString());
                found = true;
                break;
            }
        }
        if (!found && !noms_genre.isEmpty())
            genreList.append(noms_genre[0].toObject()["text"].toString());
    }
    result.genres = genreList.join(", ");

    // Rating (ScreenScraper 0-20 → 0-5)
    QString noteStr = jeu["note"].toObject()["text"].toString();
    if (!noteStr.isEmpty()) {
        bool ok;
        double note = noteStr.toDouble(&ok);
        if (ok)
            result.rating = qBound(0.0, note / 4.0, 5.0);
    }

    // Players
    result.players = jeu["joueurs"].toObject()["text"].toString();

    // Notify caller with metadata before starting slow media downloads
    result.success = !result.title.isEmpty();
    if (onMetadata && result.success)
        onMetadata(result);

    // Check cancel before starting media downloads
    if (cancelFlag && cancelFlag->loadRelaxed()) {
        return result;
    }

    // --- Media download (parallel) ---

    QJsonArray medias = jeu["medias"].toArray();
    static const QStringList regionPriority = {"us", "wor", "eu", "jp"};

    // Auth params to append to every media URL
    QString authSuffix = QString("devid=%1&devpassword=%2&softname=%3&ssid=%4&sspassword=%5")
        .arg(m_devId, m_devPassword, m_softname, m_userId, m_userPassword);

    // Build list of all media to download
    QVector<MediaDownload> downloads;

    for (const QString& mediaType : mediaTypes) {
        QStringList apiTypes = screenScraperMediaTypes(mediaType);
        if (apiTypes.isEmpty()) {
            qWarning() << "[Scraper] Unknown media type:" << mediaType;
            continue;
        }

        QString mediaUrl;

        // Try each API type for this folder (e.g. miximages tries mixrbv1 then mixrbv2)
        for (const QString& apiType : apiTypes) {
            // Region priority pass
            for (const QString& region : regionPriority) {
                for (const auto& media : medias) {
                    QJsonObject m = media.toObject();
                    if (m["type"].toString() == apiType && m["region"].toString() == region) {
                        mediaUrl = m["url"].toString();
                        break;
                    }
                }
                if (!mediaUrl.isEmpty()) break;
            }

            // Fallback: any region
            if (mediaUrl.isEmpty()) {
                for (const auto& media : medias) {
                    QJsonObject m = media.toObject();
                    if (m["type"].toString() == apiType) {
                        mediaUrl = m["url"].toString();
                        break;
                    }
                }
            }

            if (!mediaUrl.isEmpty()) break;
        }

        if (mediaUrl.isEmpty()) {
            qInfo() << "[Scraper] No media of type" << mediaType << "for:" << game.title;
            continue;
        }

        // Determine file extension: use media type as hint, then refine from URL
        QString ext;
        if (mediaType == "videos")
            ext = "mp4";
        else if (mediaType == "manuals")
            ext = "pdf";
        else
            ext = "png";

        QString urlLower = mediaUrl.toLower();
        if (urlLower.contains(".jpg") || urlLower.contains(".jpeg"))
            ext = "jpg";
        else if (urlLower.contains(".mp4"))
            ext = "mp4";
        else if (urlLower.contains(".pdf"))
            ext = "pdf";

        // Build destination path: {mediaBaseDir}/{system}/{mediaType}/{title}.{ext}
        QString destDir = mediaBaseDir + "/" + game.system + "/" + mediaType;
        QDir().mkpath(destDir);
        QString destPath = destDir + "/" + game.title + "." + ext;

        // Append auth to URL
        QString authenticatedUrl = mediaUrl;
        authenticatedUrl += (authenticatedUrl.contains('?') ? "&" : "?") + authSuffix;

        downloads.append({mediaType, authenticatedUrl, destPath});
    }

    // Fire all downloads in parallel
    QMap<QString, QString> downloaded = downloadMediaParallel(downloads, cancelFlag);
    result.mediaPaths = downloaded;

    // Success if we got metadata or at least one media file
    result.success = !result.title.isEmpty() || !downloaded.isEmpty();
    if (!result.success)
        result.error = "No metadata or media found for: " + game.title;

    return result;
}

bool Scraper::validateCredentials(const QString& userId, const QString& userPassword) {
    QUrlQuery query;
    query.addQueryItem("devid",       m_devId);
    query.addQueryItem("devpassword", m_devPassword);
    query.addQueryItem("softname",    m_softname);
    query.addQueryItem("ssid",        userId);
    query.addQueryItem("sspassword",  userPassword);
    query.addQueryItem("output",      "json");

    QString url = API_BASE + "ssuserInfos.php?" + query.toString(QUrl::FullyEncoded);

    qInfo() << "[Scraper] Validating credentials for user:" << userId;

    QByteArray data = httpGet(url);
    if (data.isEmpty()) {
        qWarning() << "[Scraper] No response during credential validation";
        return false;
    }

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull()) {
        qWarning() << "[Scraper] JSON parse error during validation:" << parseErr.errorString();
        return false;
    }

    QJsonObject root     = doc.object();
    QJsonObject response = root["response"].toObject();

    // Valid if the response contains a ssuser object
    return !response["ssuser"].toObject().isEmpty();
}

QByteArray Scraper::httpGet(const QString& url, QAtomicInt* cancelFlag) {
    // This function spins a nested QEventLoop to wait for the reply. If that
    // happens on the GUI thread while a QML signal handler is on the stack,
    // the event loop can process DeferredDelete events that tear down the
    // handler's QObject, causing QQmlData::destroyed to qFatal. Every caller
    // must therefore reach here from a worker thread (QtConcurrent::run or
    // equivalent). Enforce it explicitly to catch future regressions.
    if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
        qCritical() << "[Scraper] httpGet called on the GUI thread — this is unsafe "
                       "from QML signal handlers. Move the call to a worker thread.";
        return {};
    }

    QNetworkAccessManager mgr;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, m_softname);

    QNetworkReply* reply = mgr.get(req);

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(30000);

    // Check for cancel periodically during the request
    QTimer cancelTimer;
    if (cancelFlag) {
        QObject::connect(&cancelTimer, &QTimer::timeout, &loop, [&]() {
            if (cancelFlag->loadRelaxed())
                reply->abort();
        });
        cancelTimer.start(200);
    }

    loop.exec();

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError) {
        data = reply->readAll();
    } else {
        qWarning() << "[Scraper] HTTP error:" << reply->errorString();
    }

    reply->deleteLater();
    return data;
}

QMap<QString, QString> Scraper::downloadMediaParallel(const QVector<MediaDownload>& downloads,
                                                       QAtomicInt* cancelFlag) {
    if (downloads.isEmpty())
        return {};

    QNetworkAccessManager mgr;
    QEventLoop loop;
    QMap<QString, QString> results;
    QVector<QNetworkReply*> activeReplies;
    int pending = downloads.size();

    for (const auto& dl : downloads) {
        QNetworkRequest req{QUrl(dl.url)};
        req.setHeader(QNetworkRequest::UserAgentHeader, m_softname);

        QNetworkReply* reply = mgr.get(req);
        activeReplies.append(reply);

        // Capture dl by value for the lambda
        QString mediaType = dl.mediaType;
        QString destPath = dl.destPath;

        QObject::connect(reply, &QNetworkReply::finished, &loop, [&, reply, mediaType, destPath]() {
            activeReplies.removeOne(reply);

            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                if (!data.isEmpty()) {
                    QFile file(destPath);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(data);
                        file.close();
                        results[mediaType] = destPath;
                        qInfo() << "[Scraper] Downloaded" << mediaType;
                    } else {
                        qWarning() << "[Scraper] Cannot write:" << destPath;
                    }
                }
            } else if (reply->error() != QNetworkReply::OperationCanceledError) {
                qWarning() << "[Scraper] Download failed:" << mediaType << reply->errorString();
            }

            reply->deleteLater();
            pending--;

            if (pending <= 0)
                loop.quit();
        });
    }

    // Check for cancel periodically
    QTimer cancelTimer;
    if (cancelFlag) {
        QObject::connect(&cancelTimer, &QTimer::timeout, &loop, [&]() {
            if (cancelFlag->loadRelaxed()) {
                // Abort all pending replies — finished signal will fire for each
                for (auto* reply : activeReplies) {
                    if (reply->isRunning())
                        reply->abort();
                }
            }
        });
        cancelTimer.start(200);
    }

    // Timeout safety: abort all remaining after 60s
    QTimer overallTimeout;
    overallTimeout.setSingleShot(true);
    QObject::connect(&overallTimeout, &QTimer::timeout, &loop, [&]() {
        qWarning() << "[Scraper] Media download timed out, aborting" << activeReplies.size() << "remaining";
        for (auto* reply : activeReplies) {
            if (reply->isRunning())
                reply->abort();
        }
    });
    overallTimeout.start(60000);

    loop.exec();
    return results;
}
