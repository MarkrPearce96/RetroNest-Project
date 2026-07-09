#include <QtTest>
#include "services/github_credentials.h"
#include "services/installer_utils.h"
#include <QJsonObject>

class TestGitHubDownloadAuth : public QObject {
    Q_OBJECT
private slots:
    // With no dev_credentials.cmake (the test target defines no token),
    // the accessor must return empty — the "public build" path.
    void tokenEmptyByDefault() {
        QCOMPARE(GitHubCredentials::token(), QString());
    }

    // Public asset → browser_download_url; private asset → the API url
    // (browser_download_url won't authenticate for a private repo).
    void assetUrlSelection() {
        QJsonObject asset;
        asset["url"] = "https://api.github.com/repos/o/r/releases/assets/42";
        asset["browser_download_url"] = "https://github.com/o/r/releases/download/v1/x.zip";

        QCOMPARE(InstallerUtils::chooseAssetDownloadUrl(asset, /*isPrivate=*/false),
                 QString("https://github.com/o/r/releases/download/v1/x.zip"));
        QCOMPARE(InstallerUtils::chooseAssetDownloadUrl(asset, /*isPrivate=*/true),
                 QString("https://api.github.com/repos/o/r/releases/assets/42"));
    }
};

QTEST_APPLESS_MAIN(TestGitHubDownloadAuth)
#include "test_github_download_auth.moc"
