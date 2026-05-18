#include "installer_utils.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace InstallerUtils {

QByteArray httpGet(const QString& url, int timeoutMs, const QString& context) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeout.start(timeoutMs);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << context << "HTTP error:" << reply->errorString();
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

QString computeSha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return QString::fromLatin1(hash.result().toHex());
}

bool verifySha256(const QString& path, const QString& expected,
                  const QString& context) {
    if (expected.isEmpty()) return true;  // verification opt-in only
    const QString actual = computeSha256(path);
    if (actual.isEmpty()) {
        qWarning() << context << "SHA256: failed to read" << path;
        return false;
    }
    if (actual.compare(expected, Qt::CaseInsensitive) != 0) {
        qWarning() << context << "SHA256 MISMATCH for" << path
                   << "expected" << expected << "got" << actual;
        return false;
    }
    qInfo() << context << "SHA256 verified for" << path;
    return true;
}

} // namespace InstallerUtils
