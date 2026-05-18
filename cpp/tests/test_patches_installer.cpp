#include "services/patches_installer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>

static int failed = 0;
#define CHECK(cond) do { \
    if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond << "\n"; ++failed; } \
} while (0)

static void writeBlob(const QString& path, const QByteArray& data = "x") {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); f.open(QIODevice::WriteOnly); f.write(data);
}

static void writeSidecar(const QString& path, const QString& installedAt) {
    QFile f(path); f.open(QIODevice::WriteOnly);
    f.write(("tag=v1.0\npublished_at=2026-01-01T00:00:00Z\n"
             "installed_at=" + installedAt + "\nsha256=\n").toUtf8());
}

static void test_missing_zip_triggers_fetch() {
    QTemporaryDir tmp;
    PatchesInstaller inst;
    CHECK(inst.isFetchNeeded(tmp.path()));
}

static void test_fresh_zip_and_sidecar_skips_fetch() {
    QTemporaryDir tmp;
    writeBlob(tmp.path() + "/patches.zip");
    writeSidecar(tmp.path() + "/patches.zip.version",
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    PatchesInstaller inst;
    CHECK(!inst.isFetchNeeded(tmp.path()));
}

static void test_stale_sidecar_triggers_fetch() {
    QTemporaryDir tmp;
    writeBlob(tmp.path() + "/patches.zip");
    writeSidecar(tmp.path() + "/patches.zip.version",
                 "2024-01-01T00:00:00Z");  // > 90 days old
    PatchesInstaller inst;
    CHECK(inst.isFetchNeeded(tmp.path()));
}

static void test_zip_present_sidecar_absent_recent_mtime_respects_user() {
    QTemporaryDir tmp;
    writeBlob(tmp.path() + "/patches.zip");
    // No sidecar. mtime is "now" by default.
    PatchesInstaller inst;
    CHECK(!inst.isFetchNeeded(tmp.path()));  // respect user-placed file
}

static void test_zip_present_sidecar_absent_old_mtime_triggers_fetch() {
    QTemporaryDir tmp;
    const QString zip = tmp.path() + "/patches.zip";
    writeBlob(zip);
    // setFileTime requires the file to be open on macOS (QFileDevice constraint).
    QFile f(zip);
    f.open(QIODevice::ReadWrite);
    QDateTime old = QDateTime::currentDateTimeUtc().addDays(-100);
    f.setFileTime(old, QFileDevice::FileModificationTime);
    f.close();
    PatchesInstaller inst;
    CHECK(inst.isFetchNeeded(tmp.path()));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_missing_zip_triggers_fetch();
    test_fresh_zip_and_sidecar_skips_fetch();
    test_stale_sidecar_triggers_fetch();
    test_zip_present_sidecar_absent_recent_mtime_respects_user();
    test_zip_present_sidecar_absent_old_mtime_triggers_fetch();
    if (failed) { std::cerr << failed << " test(s) failed\n"; return 1; }
    std::cout << "All installer staleness tests passed.\n";
    return 0;
}
