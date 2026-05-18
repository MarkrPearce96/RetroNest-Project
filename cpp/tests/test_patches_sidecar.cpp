#include "services/patches_sidecar.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <cassert>
#include <iostream>

static int failed = 0;

#define CHECK(cond) do { \
    if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond << "\n"; ++failed; } \
} while (0)

static void writeFile(const QString& path, const QString& content) {
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(content.toUtf8());
}

static void test_round_trip() {
    QTemporaryDir tmp;
    const QString path = tmp.path() + "/patches.zip.version";

    PatchesSidecar in;
    in.tag = "v2026.04.15";
    in.publishedAt = "2026-04-15T14:30:00Z";
    in.installedAt = "2026-05-18T10:15:42Z";
    in.sha256 = "deadbeef";
    CHECK(PatchesSidecar::write(path, in));

    auto out = PatchesSidecar::read(path);
    CHECK(out.has_value());
    CHECK(out->tag == in.tag);
    CHECK(out->publishedAt == in.publishedAt);
    CHECK(out->installedAt == in.installedAt);
    CHECK(out->sha256 == in.sha256);
}

static void test_missing_file() {
    auto out = PatchesSidecar::read("/nonexistent/path");
    CHECK(!out.has_value());
}

static void test_tolerates_malformed() {
    QTemporaryDir tmp;
    const QString path = tmp.path() + "/patches.zip.version";
    writeFile(path,
        "tag=v1.0\n"
        "this line has no equals sign\n"
        "=value-without-key\n"
        "installed_at=2026-05-18T00:00:00Z\n");

    auto out = PatchesSidecar::read(path);
    CHECK(out.has_value());
    CHECK(out->tag == "v1.0");
    CHECK(out->installedAt == "2026-05-18T00:00:00Z");
    CHECK(out->publishedAt.isEmpty());
    CHECK(out->sha256.isEmpty());
}

static void test_empty_file() {
    QTemporaryDir tmp;
    const QString path = tmp.path() + "/patches.zip.version";
    writeFile(path, "");

    auto out = PatchesSidecar::read(path);
    CHECK(out.has_value());
    CHECK(out->tag.isEmpty());
    CHECK(out->installedAt.isEmpty());
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_round_trip();
    test_missing_file();
    test_tolerates_malformed();
    test_empty_file();
    if (failed) { std::cerr << failed << " test(s) failed\n"; return 1; }
    std::cout << "All sidecar tests passed.\n";
    return 0;
}
