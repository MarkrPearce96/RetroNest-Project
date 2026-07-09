#include <QtTest>
#include "services/github_credentials.h"

class TestGitHubDownloadAuth : public QObject {
    Q_OBJECT
private slots:
    // With no dev_credentials.cmake (the test target defines no token),
    // the accessor must return empty — the "public build" path.
    void tokenEmptyByDefault() {
        QCOMPARE(GitHubCredentials::token(), QString());
    }
};

QTEST_APPLESS_MAIN(TestGitHubDownloadAuth)
#include "test_github_download_auth.moc"
