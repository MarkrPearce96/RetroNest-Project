# PCSX2 patches.zip shipment — implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Auto-fetch `patches.zip` from `github.com/PCSX2/pcsx2_patches/releases/latest` into the PCSX2 libretro core's resources dir so the existing Phase 4 patches knobs (widescreen, no-interlacing, per-game game-fix DB) have a data file to act on.

**Architecture:** New `PatchesInstaller` service in `RetroNest-Project/cpp/src/services/` mirrors the async pipeline of `EmulatorInstaller` but is single-purpose. Two genuinely-shared helpers (`httpGet`, `verifySha256`) extracted into `installer_utils`. App startup runs a staleness check and kicks off a background fetch via `QtConcurrent::run`; `QCoreApplication::aboutToQuit` cancels in-flight fetches. A "Patches" card on the PCSX2 settings hub gives users a manual refresh path. Success/failure surfaces via `AppController::raInfoToast` (already wired to QML toast component).

**Tech Stack:** Qt 6 (QObject, QNetworkAccessManager, QtConcurrent), C++17, CMake. Test binary built with the existing `tools/` pattern (Qt-test or plain executable with assertions).

**Spec:** [2026-05-18-pcsx2-patches-shipment-design.md](../specs/2026-05-18-pcsx2-patches-shipment-design.md)

---

## Task 1: Extract shared installer helpers into `installer_utils`

**Files:**
- Create: `RetroNest-Project/cpp/src/services/installer_utils.h`
- Create: `RetroNest-Project/cpp/src/services/installer_utils.cpp`
- Modify: `RetroNest-Project/cpp/src/services/emulator_installer.h` (remove the three method declarations being moved)
- Modify: `RetroNest-Project/cpp/src/services/emulator_installer.cpp` (replace internal callers with `InstallerUtils::*` calls)
- Modify: `RetroNest-Project/cpp/CMakeLists.txt` (add new files to the build)

This is a mechanical extraction. The two helpers `EmulatorInstaller::computeSha256` and `EmulatorInstaller::verifySha256` are already `static` and have no internal coupling. We also extract `httpGet` (currently a synchronous helper inside `emulator_installer.cpp`). Atomic rename is small enough to inline at call sites; do NOT extract it.

- [ ] **Step 1: Create `installer_utils.h`**

```cpp
#pragma once

#include <QByteArray>
#include <QString>

namespace InstallerUtils {

/** Synchronous HTTP GET. Returns body on success, empty on failure.
 *  Times out after `timeoutMs` (default 30s). Logs failures with the
 *  given `context` prefix (e.g. "[Installer]", "[Patches]"). */
QByteArray httpGet(const QString& url, int timeoutMs = 30000,
                   const QString& context = QStringLiteral("[InstallerUtils]"));

/** Lower-case hex SHA256 of the file at `path`. Empty on read failure. */
QString computeSha256(const QString& path);

/** True if `expected` is empty (skip verify) or matches the file's SHA256. */
bool verifySha256(const QString& path, const QString& expected);

} // namespace InstallerUtils
```

- [ ] **Step 2: Create `installer_utils.cpp` by lifting code from `emulator_installer.cpp`**

Copy the existing implementations of `computeSha256`, `verifySha256`, and `httpGet` from `emulator_installer.cpp` into `installer_utils.cpp` wrapped in `namespace InstallerUtils {}`. `httpGet` currently has hardcoded `"[Installer]"` log prefixes; replace those with the `context` parameter.

```cpp
#include "installer_utils.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtLogging>

namespace InstallerUtils {

QByteArray httpGet(const QString& url, int timeoutMs, const QString& context) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setRawHeader("Accept", "application/json");

    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!timer.isActive()) {
        qWarning().noquote() << context << "HTTP timeout for" << url;
        reply->abort();
        reply->deleteLater();
        return {};
    }
    if (reply->error() != QNetworkReply::NoError) {
        qWarning().noquote() << context << "HTTP error" << reply->errorString()
                              << "for" << url;
        reply->deleteLater();
        return {};
    }
    QByteArray body = reply->readAll();
    reply->deleteLater();
    return body;
}

QString computeSha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return {};
    return QString::fromLatin1(h.result().toHex());
}

bool verifySha256(const QString& path, const QString& expected) {
    if (expected.isEmpty()) return true;
    const QString actual = computeSha256(path);
    return actual.compare(expected, Qt::CaseInsensitive) == 0;
}

} // namespace InstallerUtils
```

(Verify the exact existing implementation of `httpGet` first — if `emulator_installer.cpp`'s version differs, copy *its* logic verbatim and add only the `context` parameter swap.)

- [ ] **Step 3: Update `emulator_installer.h` and `.cpp` to call the new namespace**

In `emulator_installer.h`, delete the three declarations:
```cpp
static QString computeSha256(const QString& path);
static bool verifySha256(const QString& path, const QString& expected);
```
(plus any `httpGet` declaration if it exists in the header).

In `emulator_installer.cpp`:
- Add `#include "installer_utils.h"` at the top.
- Delete the function bodies for `computeSha256`, `verifySha256`, and `httpGet`.
- Replace all call sites `verifySha256(...)` → `InstallerUtils::verifySha256(...)`, `httpGet(...)` → `InstallerUtils::httpGet(url, 30000, "[Installer]")`.

- [ ] **Step 4: Add new files to CMakeLists.txt**

Find the `add_executable(RetroNest ...)` or `target_sources(RetroNest PRIVATE ...)` block in `cpp/CMakeLists.txt` that lists `services/emulator_installer.cpp`. Add `services/installer_utils.cpp` next to it. Headers are typically picked up automatically; if they're explicitly listed, add `services/installer_utils.h` too.

- [ ] **Step 5: Build and verify no regression**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target RetroNest
```

Expected: clean build. If existing emulator install paths break, the call-site substitutions in Step 3 are wrong — re-check each replaced call.

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/src/services/installer_utils.h cpp/src/services/installer_utils.cpp \
        cpp/src/services/emulator_installer.h cpp/src/services/emulator_installer.cpp \
        cpp/CMakeLists.txt
git commit -m "refactor(services): extract httpGet/sha256 helpers into installer_utils

Mechanical extraction of three static helpers from EmulatorInstaller into a
shared namespace. No behavior change. Prepares for the PatchesInstaller
sub-project which reuses the same primitives."
```

---

## Task 2: Sidecar parser/writer for `patches.zip.version`

**Files:**
- Create: `RetroNest-Project/cpp/src/services/patches_sidecar.h`
- Create: `RetroNest-Project/cpp/src/services/patches_sidecar.cpp`
- Create: `RetroNest-Project/cpp/tools/test_patches_sidecar.cpp`
- Modify: `RetroNest-Project/cpp/CMakeLists.txt`

The sidecar is `key=value\n` text. Format defined in the spec:
```
tag=v2026.04.15
published_at=2026-04-15T14:30:00Z
installed_at=2026-05-18T10:15:42Z
sha256=<hex or empty>
```

Parser must tolerate missing keys, empty files, malformed lines (skip them).

- [ ] **Step 1: Write the failing test**

Create `cpp/tools/test_patches_sidecar.cpp`:

```cpp
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
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target test_patches_sidecar 2>&1 | head -20
```

Expected: FAIL with "patches_sidecar.h: No such file" or "PatchesSidecar undeclared" — header not created yet.

- [ ] **Step 3: Create `patches_sidecar.h`**

```cpp
#pragma once

#include <QString>
#include <optional>

struct PatchesSidecar {
    QString tag;          // GitHub release tag_name, e.g. "v2026.04.15"
    QString publishedAt;  // GitHub published_at ISO timestamp
    QString installedAt;  // local install time (Qt::ISODateWithMs)
    QString sha256;       // lower-case hex; empty if upstream gave no digest

    /** Read `path` and parse key=value lines. Tolerates missing keys,
     *  empty files, malformed lines. Returns nullopt only if the file
     *  cannot be opened for reading. */
    static std::optional<PatchesSidecar> read(const QString& path);

    /** Atomic write: writes to `path + ".tmp"` then renames. Returns false
     *  on any I/O error. */
    static bool write(const QString& path, const PatchesSidecar& s);
};
```

- [ ] **Step 4: Create `patches_sidecar.cpp`**

```cpp
#include "patches_sidecar.h"

#include <QFile>
#include <QTextStream>

std::optional<PatchesSidecar> PatchesSidecar::read(const QString& path) {
    QFile f(path);
    if (!f.exists()) return std::nullopt;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return std::nullopt;

    PatchesSidecar out;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        const int eq = line.indexOf('=');
        if (eq <= 0) continue;  // skip "no equals" and "=value-without-key"
        const QString key = line.left(eq).trimmed();
        const QString val = line.mid(eq + 1).trimmed();
        if (key == "tag")             out.tag = val;
        else if (key == "published_at") out.publishedAt = val;
        else if (key == "installed_at") out.installedAt = val;
        else if (key == "sha256")       out.sha256 = val;
        // unknown keys silently ignored (forward-compat)
    }
    return out;
}

bool PatchesSidecar::write(const QString& path, const PatchesSidecar& s) {
    const QString tmp = path + ".tmp";
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return false;
        QTextStream out(&f);
        out << "tag=" << s.tag << "\n"
            << "published_at=" << s.publishedAt << "\n"
            << "installed_at=" << s.installedAt << "\n"
            << "sha256=" << s.sha256 << "\n";
    }
    // QFile::rename refuses to overwrite an existing destination on some
    // platforms; remove first.
    QFile::remove(path);
    return QFile::rename(tmp, path);
}
```

- [ ] **Step 5: Add test binary to CMakeLists.txt**

In `cpp/CMakeLists.txt`, find the existing `add_executable(test_core_options ...)` or similar test-binary block. Add a sibling:

```cmake
add_executable(test_patches_sidecar
    tools/test_patches_sidecar.cpp
    src/services/patches_sidecar.cpp
)
target_link_libraries(test_patches_sidecar PRIVATE Qt6::Core)
target_include_directories(test_patches_sidecar PRIVATE src)
```

Also add `services/patches_sidecar.cpp` (and `.h` if listed) to the main `RetroNest` target's sources.

- [ ] **Step 6: Run the test and verify it passes**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target test_patches_sidecar
./build-arm64/test_patches_sidecar
```

Expected: `All sidecar tests passed.` exit code 0.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/services/patches_sidecar.h cpp/src/services/patches_sidecar.cpp \
        cpp/tools/test_patches_sidecar.cpp cpp/CMakeLists.txt
git commit -m "feat(services): patches.zip.version sidecar reader/writer

Key=value text format with tag/published_at/installed_at/sha256.
Tolerates missing keys, empty files, malformed lines. Atomic write
via .tmp + rename. Unit-tested in tools/test_patches_sidecar."
```

---

## Task 3: `PatchesInstaller` skeleton + `isFetchNeeded()` with tests

**Files:**
- Create: `RetroNest-Project/cpp/src/services/patches_installer.h`
- Create: `RetroNest-Project/cpp/src/services/patches_installer.cpp`
- Create: `RetroNest-Project/cpp/tools/test_patches_installer.cpp`
- Modify: `RetroNest-Project/cpp/CMakeLists.txt`

Per spec, staleness fires when:
- `patches.zip` is missing, OR
- (zip + sidecar both present) AND `(now - installed_at) > 90 days`, OR
- (zip present + sidecar absent) AND `(now - zip mtime) > 90 days`.

A user-placed zip with no sidecar is respected for 90 days. After fetch resolves the upstream tag, if sidecar tag matches, short-circuit to "up to date" (that logic lives in Task 4's fetchAsync, not here).

- [ ] **Step 1: Write the failing test**

Create `cpp/tools/test_patches_installer.cpp`:

```cpp
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
    // Force mtime to ~100 days ago. QFileDevice has no setFileTime helper
    // for arbitrary files; use POSIX utimensat via QFile::setFileTime if
    // available (Qt 6.4+), else skip this test path.
    QFile f(zip);
    QDateTime old = QDateTime::currentDateTimeUtc().addDays(-100);
    f.setFileTime(old, QFileDevice::FileModificationTime);
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
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target test_patches_installer 2>&1 | head -20
```

Expected: FAIL — header missing.

- [ ] **Step 3: Create `patches_installer.h`**

```cpp
#pragma once

#include <QObject>
#include <QString>

/**
 * PatchesInstaller — fetches PCSX2 patches.zip from
 * github.com/PCSX2/pcsx2_patches/releases/latest into the libretro
 * core's resources directory.
 *
 * Designed for single-purpose use: PCSX2 is the only emulator in the
 * RetroNest stable that consumes an externally-released community data
 * bundle. If a second consumer ever materializes, extract a generic
 * ResourcesInstaller with the actual second use case in hand.
 */
class PatchesInstaller : public QObject {
    Q_OBJECT

public:
    /** Default age threshold for "stale" sidecar/zip (90 days). */
    static constexpr qint64 kStaleAgeSeconds = 90LL * 24 * 60 * 60;

    explicit PatchesInstaller(QObject* parent = nullptr);

    /** True if a fetch should run for the resources dir at `resourcesDir`.
     *  Pure function over the filesystem state; safe to call on main thread.
     *  Returns false if `resourcesDir` doesn't exist or its parent (the
     *  installed core dir) is missing — no point fetching patches without
     *  a core to read them. */
    bool isFetchNeeded(const QString& resourcesDir) const;

    /** Kick off the async fetch state machine on a background thread.
     *  Emits progress() during download, finished() on completion.
     *  If `force` is false and isFetchNeeded() is false, short-circuits
     *  to finished(true, "already up to date", sidecar.tag).
     *  Safe to call from the main thread. */
    void fetchAsync(const QString& resourcesDir, bool force = false);

signals:
    /** 0.0–1.0 download ratio + human-readable phase string. */
    void progress(qreal ratio, const QString& message);

    /** success=true on successful fetch OR clean short-circuit ("up to date").
     *  On failure: success=false, message is a user-facing reason. */
    void finished(bool success, const QString& message, const QString& tag);
};
```

- [ ] **Step 4: Implement `isFetchNeeded()` in `patches_installer.cpp`**

```cpp
#include "patches_installer.h"
#include "patches_sidecar.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

PatchesInstaller::PatchesInstaller(QObject* parent) : QObject(parent) {}

bool PatchesInstaller::isFetchNeeded(const QString& resourcesDir) const {
    if (resourcesDir.isEmpty()) return false;
    const QFileInfo dirInfo(resourcesDir);
    if (!dirInfo.exists()) return false;

    const QString zipPath = resourcesDir + "/patches.zip";
    const QString sidecarPath = zipPath + ".version";

    const QFileInfo zipInfo(zipPath);
    if (!zipInfo.exists()) return true;  // zip missing → fetch

    const auto sidecar = PatchesSidecar::read(sidecarPath);
    const QDateTime now = QDateTime::currentDateTimeUtc();

    if (sidecar.has_value() && !sidecar->installedAt.isEmpty()) {
        const QDateTime installed = QDateTime::fromString(
            sidecar->installedAt, Qt::ISODate);
        if (!installed.isValid()) return true;  // malformed = treat as stale
        return installed.secsTo(now) > kStaleAgeSeconds;
    }

    // Zip present, sidecar absent or unusable → user-placed file.
    // Respect it until zip mtime crosses staleness threshold.
    const QDateTime zipMtime = zipInfo.lastModified().toUTC();
    return zipMtime.secsTo(now) > kStaleAgeSeconds;
}

void PatchesInstaller::fetchAsync(const QString& resourcesDir, bool force) {
    // Implemented in Task 4. Stub for now so this file compiles.
    Q_UNUSED(resourcesDir);
    Q_UNUSED(force);
    emit finished(false, "fetchAsync not yet implemented", {});
}
```

- [ ] **Step 5: Add test binary + main-target source to CMakeLists.txt**

```cmake
add_executable(test_patches_installer
    tools/test_patches_installer.cpp
    src/services/patches_installer.cpp
    src/services/patches_sidecar.cpp
)
target_link_libraries(test_patches_installer PRIVATE Qt6::Core)
target_include_directories(test_patches_installer PRIVATE src)
```

Also add `services/patches_installer.cpp` to the main RetroNest target sources.

- [ ] **Step 6: Run the test and verify it passes**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target test_patches_installer
./build-arm64/test_patches_installer
```

Expected: `All installer staleness tests passed.` exit code 0.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/services/patches_installer.h cpp/src/services/patches_installer.cpp \
        cpp/tools/test_patches_installer.cpp cpp/CMakeLists.txt
git commit -m "feat(services): PatchesInstaller skeleton + isFetchNeeded staleness logic

Staleness rules per spec: zip missing → fetch; zip+sidecar present + installed_at
> 90 days → fetch; zip present + sidecar absent + mtime > 90 days → fetch
(respects user-placed files within the 90-day window). fetchAsync stubbed,
implemented in next commit."
```

---

## Task 4: Implement `fetchAsync()` state machine

**Files:**
- Modify: `RetroNest-Project/cpp/src/services/patches_installer.h`
- Modify: `RetroNest-Project/cpp/src/services/patches_installer.cpp`

This is the network-heavy path. Pure unit tests would require an injectable HTTP layer; per the spec we cover this with the smoke test (Task 8) rather than building HTTP mocks. The code is small enough that visual review + the integration smoke is the right cost trade.

The state machine:
1. If not force and !isFetchNeeded → short-circuit to `finished(true, "already up to date", tag)`.
2. GET `https://api.github.com/repos/PCSX2/pcsx2_patches/releases/latest` via `InstallerUtils::httpGet`.
3. Parse JSON, locate the asset named exactly `patches.zip`.
4. Compare release `tag_name` to sidecar `tag` — if match AND zip is present, short-circuit.
5. Download asset URL to `<resourcesDir>/patches.zip.tmp` via `QNetworkAccessManager` (signal-based, can be aborted by `aboutToQuit`).
6. SHA256 verify if digest provided.
7. Atomic rename `.tmp` → `patches.zip`.
8. Write updated sidecar.
9. Emit `finished(true, "Updated to <tag>", tag)`.

- [ ] **Step 1: Add private helpers to the header**

In `patches_installer.h`, inside the `private:` section, add:

```cpp
private:
    struct ReleaseInfo {
        bool ok = false;
        QString errorMessage;
        QString tagName;
        QString publishedAt;
        QString downloadUrl;
        QString sha256;  // empty = no digest provided
    };

    /** Hit the GitHub API and resolve the patches.zip asset. Synchronous;
     *  call from a background thread. */
    ReleaseInfo fetchReleaseInfo() const;

    /** Synchronous download of `url` to `destPath` using a fresh
     *  QNetworkAccessManager. Honors QCoreApplication::aboutToQuit
     *  for clean cancellation. */
    bool downloadTo(const QString& url, const QString& destPath);

    /** Run the complete fetch pipeline. Called on a worker thread by
     *  fetchAsync()'s QtConcurrent::run. */
    void runFetch(const QString& resourcesDir, bool force);
```

- [ ] **Step 2: Replace the `fetchAsync` stub and add the helpers**

In `patches_installer.cpp`, add at the top:

```cpp
#include "installer_utils.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent>
```

Replace the existing `fetchAsync()` stub with:

```cpp
void PatchesInstaller::fetchAsync(const QString& resourcesDir, bool force) {
    QtConcurrent::run([this, resourcesDir, force]() {
        runFetch(resourcesDir, force);
    });
}

void PatchesInstaller::runFetch(const QString& resourcesDir, bool force) {
    if (!force && !isFetchNeeded(resourcesDir)) {
        const auto sc = PatchesSidecar::read(resourcesDir + "/patches.zip.version");
        emit finished(true, "Patches already up to date",
                      sc.has_value() ? sc->tag : QString{});
        return;
    }

    emit progress(0.0, "Fetching release info");
    ReleaseInfo info = fetchReleaseInfo();
    if (!info.ok) {
        emit finished(false, info.errorMessage, {});
        return;
    }

    // Tag short-circuit: if sidecar matches latest tag AND zip is present,
    // no need to re-download.
    const QString zipPath = resourcesDir + "/patches.zip";
    const QString sidecarPath = zipPath + ".version";
    if (QFile::exists(zipPath)) {
        const auto sc = PatchesSidecar::read(sidecarPath);
        if (sc.has_value() && sc->tag == info.tagName && !info.tagName.isEmpty()) {
            // Refresh installed_at so staleness clock resets.
            PatchesSidecar refreshed = *sc;
            refreshed.installedAt = QDateTime::currentDateTimeUtc()
                                        .toString(Qt::ISODate);
            PatchesSidecar::write(sidecarPath, refreshed);
            emit finished(true, "Patches already up to date", info.tagName);
            return;
        }
    }

    emit progress(0.1, "Downloading patches.zip");
    const QString tmpPath = zipPath + ".tmp";
    QDir().mkpath(resourcesDir);
    if (!downloadTo(info.downloadUrl, tmpPath)) {
        QFile::remove(tmpPath);
        emit finished(false, "Download failed", info.tagName);
        return;
    }

    if (!InstallerUtils::verifySha256(tmpPath, info.sha256)) {
        QFile::remove(tmpPath);
        emit finished(false, "SHA256 verification failed", info.tagName);
        return;
    }

    QFile::remove(zipPath);
    if (!QFile::rename(tmpPath, zipPath)) {
        QFile::remove(tmpPath);
        emit finished(false, "Could not finalize patches.zip on disk",
                      info.tagName);
        return;
    }

    PatchesSidecar updated;
    updated.tag = info.tagName;
    updated.publishedAt = info.publishedAt;
    updated.installedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    updated.sha256 = info.sha256;
    PatchesSidecar::write(sidecarPath, updated);

    emit finished(true,
                  QString("Patches updated to %1").arg(info.tagName),
                  info.tagName);
}

PatchesInstaller::ReleaseInfo PatchesInstaller::fetchReleaseInfo() const {
    ReleaseInfo info;
    const QString apiUrl =
        "https://api.github.com/repos/PCSX2/pcsx2_patches/releases/latest";
    const QByteArray body =
        InstallerUtils::httpGet(apiUrl, 30000, "[Patches]");
    if (body.isEmpty()) {
        info.errorMessage = "Could not reach GitHub";
        return info;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        info.errorMessage = "Invalid release JSON from GitHub";
        return info;
    }
    const QJsonObject release = doc.object();
    info.tagName = release["tag_name"].toString();
    info.publishedAt = release["published_at"].toString();

    for (const auto& a : release["assets"].toArray()) {
        const QJsonObject asset = a.toObject();
        if (asset["name"].toString() == "patches.zip") {
            info.downloadUrl = asset["browser_download_url"].toString();
            const QString digest = asset["digest"].toString();
            if (digest.startsWith("sha256:", Qt::CaseInsensitive))
                info.sha256 = digest.mid(7).toLower();
            info.ok = !info.downloadUrl.isEmpty();
            if (!info.ok) info.errorMessage = "Asset patches.zip has no download URL";
            return info;
        }
    }
    info.errorMessage = "No patches.zip asset in latest release";
    return info;
}

bool PatchesInstaller::downloadTo(const QString& url, const QString& destPath) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = nam.get(req);

    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reply->abort();
        reply->deleteLater();
        return false;
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::readyRead, &out, [&]() {
        out.write(reply->readAll());
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, this,
                     [this](qint64 r, qint64 t) {
        if (t > 0) emit progress(0.1 + 0.9 * (qreal(r) / qreal(t)),
                                  "Downloading patches.zip");
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // Cmd+Q during fetch — abort cleanly so the threadpool can join.
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, reply,
                     &QNetworkReply::abort);

    loop.exec();

    out.write(reply->readAll());
    out.close();

    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) qWarning() << "[Patches] Download error:" << reply->errorString();
    reply->deleteLater();
    return ok;
}
```

- [ ] **Step 3: Build to verify it compiles**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target RetroNest
cmake --build build-arm64 --target test_patches_installer
./build-arm64/test_patches_installer
```

Expected: clean build; sidecar/staleness tests still pass (Task 3 coverage unchanged; fetchAsync isn't exercised by unit tests).

- [ ] **Step 4: Commit**

```bash
git add cpp/src/services/patches_installer.h cpp/src/services/patches_installer.cpp
git commit -m "feat(services): PatchesInstaller fetchAsync state machine

Synchronous-on-worker-thread fetch via QtConcurrent::run: GitHub releases
API → asset match → download with progress → SHA256 verify → atomic rename
→ sidecar write. aboutToQuit aborts in-flight downloads to keep Cmd+Q
shutdown clean (mirrors the 9d86291 RAClient pattern). Tag-match
short-circuit refreshes installed_at without re-downloading."
```

---

## Task 5: Resolve resources dir + wire startup hook in `main.cpp`

**Files:**
- Modify: `RetroNest-Project/cpp/src/main.cpp`

The resources dir convention (per spec): `<emulators-libretro>/cores/pcsx2_libretro_resources/`. Resolve from `Paths::emulatorsDir("libretro")`.

Hook point: after `Paths::setRoot(rootPath)` and `Paths::ensureDirectories()` succeed, before the QML main app loads. Construct a long-lived `PatchesInstaller`, kick off `fetchAsync()`. The `PatchesInstaller` instance is owned by `AppController` or main-scope to outlive the worker thread.

- [ ] **Step 1: Add includes to `main.cpp`**

At the top of `cpp/src/main.cpp`, alongside existing service includes:

```cpp
#include "services/patches_installer.h"
```

- [ ] **Step 2: Construct the installer and start the fetch**

Find the post-wizard-accept block in `main.cpp` (around line 142 per the earlier grep — right after `Paths::ensureDirectories();`). Add:

```cpp
// Auto-fetch PCSX2 patches.zip on launch — staleness-gated, non-blocking.
// Owned by an automatic-storage object so dtor runs at app exit (any in-flight
// fetch is aborted via aboutToQuit, which is wired in PatchesInstaller).
PatchesInstaller patchesInstaller;
{
    const QString libretroCoresDir =
        Paths::emulatorsDir("libretro") + "/cores";
    const QString resourcesDir =
        libretroCoresDir + "/pcsx2_libretro_resources";
    QDir().mkpath(resourcesDir);

    // Connect logging before kicking off the fetch.
    QObject::connect(&patchesInstaller, &PatchesInstaller::finished, qApp,
        [](bool success, const QString& message, const QString& tag) {
            if (success) {
                qInfo().noquote() << "[Patches]" << message
                                  << (tag.isEmpty() ? "" : "(" + tag + ")");
            } else {
                qWarning().noquote() << "[Patches]" << message;
            }
        });
    patchesInstaller.fetchAsync(resourcesDir);
}
```

Place this block AFTER `MigrationPcsx2::runIfNeeded()` and BEFORE the `Database db;` line — the fetch is independent of DB state and shouldn't block on DB open.

- [ ] **Step 3: Make the installer accessible to AppController for the toast route (Task 6)**

Pass `&patchesInstaller` to AppController's constructor or store it in a setter that AppController reads. The simplest mechanic: AppController already exists later in `main.cpp`; find its construction and pass the installer pointer. If AppController doesn't currently take optional services, add a setter:

In `cpp/src/ui/app_controller.h`, add a public method:
```cpp
void attachPatchesInstaller(PatchesInstaller* installer);
```
And a forward declaration `class PatchesInstaller;` at the top.

In `app_controller.cpp`, implement:
```cpp
#include "services/patches_installer.h"
// ...
void AppController::attachPatchesInstaller(PatchesInstaller* installer) {
    m_patchesInstaller = installer;
}
```
Add `PatchesInstaller* m_patchesInstaller = nullptr;` as a private member.

Back in `main.cpp`, after AppController is constructed:
```cpp
appController.attachPatchesInstaller(&patchesInstaller);
```

- [ ] **Step 4: Build and run; verify the fetch fires once**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target RetroNest
./build-arm64/RetroNest.app/Contents/MacOS/RetroNest 2>&1 | grep -i "patches"
```

Expected: `[Patches] Patches updated to v<tag> (v<tag>)` on first launch with no existing file. On a second launch <90 days later: `[Patches] Patches already up to date (v<tag>)`. With network disconnected and no existing file: `[Patches] Could not reach GitHub`.

- [ ] **Step 5: Verify Cmd+Q during fetch shuts down cleanly**

```bash
./build-arm64/RetroNest.app/Contents/MacOS/RetroNest &
sleep 1  # mid-fetch
osascript -e 'tell application "RetroNest" to quit'
```

Expected: process exits within ~1s; no hang; no "QThread destroyed while still running" warnings on stderr.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/main.cpp cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat(main): kick off PCSX2 patches fetch on launch

PatchesInstaller is constructed in main scope after Paths setup so it
outlives the worker thread. fetchAsync runs in QtConcurrent::run with
aboutToQuit cancellation; the worker emits progress + finished signals.
AppController gets a pointer for the upcoming toast routing (Task 6)
and the settings-dialog refresh button (Task 7)."
```

---

## Task 6: Route `finished` signal to a user-visible toast

**Files:**
- Modify: `RetroNest-Project/cpp/src/ui/app_controller.h`
- Modify: `RetroNest-Project/cpp/src/ui/app_controller.cpp`

Per spec: success toast on completion; failure toast suppressed on startup path but emitted on manual-refresh path. We use the existing `AppController::raInfoToast` signal which QML already wires to `AchievementToast.qml`. The header text "PCSX2 Patches" disambiguates from achievement unlocks.

Distinguish startup vs manual paths via a `force` flag passed to `fetchAsync()` — startup uses `force=false`, manual refresh uses `force=true`. We need that flag visible to the toast handler too. Simplest path: pass the same flag back via a wrapper method on AppController.

- [ ] **Step 1: Add a wrapper method to AppController**

In `cpp/src/ui/app_controller.h`, add:

```cpp
public slots:
    /** Triggered by the PCSX2 settings "Refresh PCSX2 patches" button (Task 7).
     *  Forces a fetch and surfaces both success AND failure toasts. */
    void refreshPcsx2Patches();

private:
    void emitPatchesToast(bool success, const QString& message,
                          bool isManualRefresh);
```

- [ ] **Step 2: Wire signal connection in AppController's constructor or `attachPatchesInstaller`**

In `app_controller.cpp`'s `attachPatchesInstaller`:

```cpp
void AppController::attachPatchesInstaller(PatchesInstaller* installer) {
    m_patchesInstaller = installer;
    if (!m_patchesInstaller) return;
    // Startup-path connection: success toast only.
    connect(m_patchesInstaller, &PatchesInstaller::finished, this,
            [this](bool success, const QString& message, const QString& /*tag*/) {
                if (success) emitPatchesToast(true, message, /*isManualRefresh*/ false);
                // Failures suppressed on startup path per spec.
            });
}

void AppController::refreshPcsx2Patches() {
    if (!m_patchesInstaller) {
        emitPatchesToast(false, "Patches installer not available", true);
        return;
    }
    const QString resourcesDir =
        Paths::emulatorsDir("libretro") + "/cores/pcsx2_libretro_resources";
    // One-shot connection for manual-refresh path: emit both success and
    // failure toasts. Disconnects itself after firing.
    auto* once = new QObject(this);
    connect(m_patchesInstaller, &PatchesInstaller::finished, once,
            [this, once](bool success, const QString& message,
                         const QString& /*tag*/) {
                emitPatchesToast(success, message, /*isManualRefresh*/ true);
                once->deleteLater();
            });
    m_patchesInstaller->fetchAsync(resourcesDir, /*force*/ true);
}

void AppController::emitPatchesToast(bool success, const QString& message,
                                      bool isManualRefresh) {
    Q_UNUSED(isManualRefresh);  // Reserved for future styling differences.
    emit raInfoToast(
        /*header*/      "PCSX2 Patches",
        /*title*/       success ? "Updated" : "Update failed",
        /*description*/ message,
        /*imageUrl*/    QString(),
        /*durationMs*/  success ? 3500 : 5000);
}
```

(Add `#include "core/paths.h"` if not already present in `app_controller.cpp`.)

- [ ] **Step 3: Build and verify the toast surface compiles**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target RetroNest
```

Expected: clean build. (Visual confirmation of the toast deferred to Task 8 smoke test.)

- [ ] **Step 4: Commit**

```bash
git add cpp/src/ui/app_controller.h cpp/src/ui/app_controller.cpp
git commit -m "feat(ui): route PatchesInstaller finished signals to in-app toast

Startup path emits success-only toast (failures stay silent so offline
users aren't nagged). Manual refresh path (Task 7 wires the button)
emits both success and failure with longer duration on failure. Reuses
the existing AchievementToast component via AppController::raInfoToast,
distinguished by 'PCSX2 Patches' header text."
```

---

## Task 7: "Patches" card on the PCSX2 settings hub with refresh button

**Files:**
- Create: `RetroNest-Project/cpp/src/ui/settings/pcsx2/pcsx2_patches_page.h`
- Create: `RetroNest-Project/cpp/src/ui/settings/pcsx2/pcsx2_patches_page.cpp`
- Modify: `RetroNest-Project/cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp`
- Modify: `RetroNest-Project/cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp`
- Modify: `RetroNest-Project/cpp/CMakeLists.txt`

A new "Patches" card on the existing 5-card grid layout (Recommended / Emulation / Graphics / Audio / Memory Cards). Activating it opens a small custom `QWidget` page (not a `GenericSettingsPage`) with a status label and a "Refresh Now" button.

The status label reads the sidecar at page-open time and shows `Currently installed: v<tag>` or `No patches.zip installed yet`. The button calls `AppController::refreshPcsx2Patches()` from Task 6.

- [ ] **Step 1: Create `pcsx2_patches_page.h`**

```cpp
#pragma once

#include <QWidget>

class AppController;
class QLabel;
class QPushButton;

/**
 * Small settings sub-page for PCSX2 patches.zip status + manual refresh.
 * Read-only status (installed tag from sidecar) plus a "Refresh Now"
 * button that delegates to AppController::refreshPcsx2Patches().
 */
class Pcsx2PatchesPage : public QWidget {
    Q_OBJECT
public:
    Pcsx2PatchesPage(AppController* app, QWidget* parent = nullptr);

private:
    void refreshStatusLabel();

    AppController* m_app = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_button = nullptr;
};
```

- [ ] **Step 2: Create `pcsx2_patches_page.cpp`**

```cpp
#include "pcsx2_patches_page.h"

#include "core/paths.h"
#include "services/patches_sidecar.h"
#include "ui/app_controller.h"
#include "ui/settings/settings_dialog_theme.h"

#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

Pcsx2PatchesPage::Pcsx2PatchesPage(AppController* app, QWidget* parent)
    : QWidget(parent), m_app(app) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto* heading = new QLabel("PCSX2 Patches", this);
    heading->setStyleSheet("font-size: 20px; font-weight: 600;");
    layout->addWidget(heading);

    auto* blurb = new QLabel(
        "Widescreen, no-interlacing, and per-game game-fix patches are "
        "maintained by the PCSX2 community at github.com/PCSX2/pcsx2_patches. "
        "RetroNest auto-checks for updates on launch (every 90 days). Use "
        "the button below to force a refresh.", this);
    blurb->setWordWrap(true);
    layout->addWidget(blurb);

    m_status = new QLabel(this);
    m_status->setStyleSheet("color: #888;");
    layout->addWidget(m_status);
    refreshStatusLabel();

    m_button = new QPushButton("Refresh PCSX2 Patches", this);
    QObject::connect(m_button, &QPushButton::clicked, this, [this]() {
        m_button->setEnabled(false);
        m_button->setText("Refreshing…");
        m_app->refreshPcsx2Patches();
        // Re-enable + restore label once the toast surfaces (driven off the
        // installer's finished signal). Simplest: re-poll the sidecar
        // periodically; the toast itself is the user-visible feedback.
        QTimer::singleShot(5000, this, [this]() {
            m_button->setEnabled(true);
            m_button->setText("Refresh PCSX2 Patches");
            refreshStatusLabel();
        });
    });
    layout->addWidget(m_button);
    layout->addStretch(1);
}

void Pcsx2PatchesPage::refreshStatusLabel() {
    const QString zipPath =
        Paths::emulatorsDir("libretro")
        + "/cores/pcsx2_libretro_resources/patches.zip";

    // Per spec non-goal: no detailed version readout in the settings UI.
    // Just installed / not-installed status so the refresh button has
    // a tiny bit of context.
    m_status->setText(QFile::exists(zipPath) ? "Status: installed"
                                              : "Status: not installed");
}
```

Add `#include <QTimer>` near the top.

- [ ] **Step 3: Add the card to `pcsx2_category_hub.cpp`**

After the "Memory Cards" card (around line 39), before `contentLayout()->addLayout(grid);`, add:

```cpp
    grid->addWidget(makeCard(QStringLiteral("\U0001F9E9"), "Patches",
                             "Widescreen / no-interlacing / game-fixes",
                             /*count*/ -1, "Patches"),
                    2, 1);
```

Note: `countSettings("Patches")` returns 0 since there's no SettingDef rows for this card. Inline-modify `Pcsx2CategoryHub::countSettings` (or, if it's an inherited method, modify `makeCard`) to treat the Patches category specially OR pass `-1` and have `makeCard` skip the count chip. Concretely: if `makeCard`'s `count` parameter is `< 0`, omit the count chip from the rendered card. Apply that change in the same commit; it's a single-line guard in `makeCard`.

- [ ] **Step 4: Wire the "Patches" route in `pcsx2_settings_dialog.cpp`**

In `Pcsx2SettingsDialog::onCategoryActivated`, after the `__controller__` early-return and before the adapter lookup, add:

```cpp
    if (category == QStringLiteral("Patches")) {
        auto* page = new Pcsx2PatchesPage(m_app, this);
        pushPage(page, /*hasSubTabs*/ false);
        return;
    }
```

Add `#include "pcsx2_patches_page.h"` near the top.

- [ ] **Step 5: Add the new sources to CMakeLists.txt**

Add `ui/settings/pcsx2/pcsx2_patches_page.cpp` (and `.h` if explicitly listed) to the main RetroNest target's sources.

- [ ] **Step 6: Build and visually verify**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project/cpp
cmake --build build-arm64 --target RetroNest
./build-arm64/RetroNest.app/Contents/MacOS/RetroNest
```

Expected: open PCSX2 settings → see 6 cards including "Patches" → click Patches → see the page with status line and refresh button. Click Refresh Now → button greys out, toast fires after the fetch completes, status label re-reads on timer.

- [ ] **Step 7: Commit**

```bash
git add cpp/src/ui/settings/pcsx2/pcsx2_patches_page.h \
        cpp/src/ui/settings/pcsx2/pcsx2_patches_page.cpp \
        cpp/src/ui/settings/pcsx2/pcsx2_category_hub.cpp \
        cpp/src/ui/settings/pcsx2/pcsx2_settings_dialog.cpp \
        cpp/CMakeLists.txt
git commit -m "feat(ui): Patches card on PCSX2 settings hub with manual refresh

New sixth card on the PCSX2 settings hub opens a small status page
showing the installed patches tag (from sidecar) and a Refresh Now
button that calls AppController::refreshPcsx2Patches(). Reuses the
existing toast surface for completion feedback."
```

---

## Task 8: Manual smoke test on clean machine state

**Files:**
- None (verification only)

This is the integration test the unit tests don't cover. Follow each step in order; halt and fix if any expectation fails.

- [ ] **Step 1: Wipe existing state**

```bash
RES=~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources
rm -f "$RES/patches.zip" "$RES/patches.zip.version" "$RES/patches.zip.tmp"
ls "$RES/"
```

Expected: directory exists; only `Roboto-Regular.ttf` (from SP7a) remains.

- [ ] **Step 2: Launch RetroNest, verify auto-fetch fires**

```bash
~/Documents/Projects/RetroNest-Project/cpp/build-arm64/RetroNest.app/Contents/MacOS/RetroNest 2>&1 \
  | grep -i "patches" &
RNPID=$!
sleep 15
ls -la "$RES/"
```

Expected: log line `[Patches] Patches updated to v<tag> (v<tag>)`. Directory now contains `patches.zip` AND `patches.zip.version`. Toast appears in app: header "PCSX2 Patches", title "Updated".

- [ ] **Step 3: Verify second-launch short-circuit**

Quit RetroNest. Relaunch:

```bash
~/Documents/Projects/RetroNest-Project/cpp/build-arm64/RetroNest.app/Contents/MacOS/RetroNest 2>&1 \
  | grep -i "patches" &
```

Expected: log line `[Patches] Patches already up to date (v<tag>)` — no re-download. No toast (silent because nothing changed; success toast still fires per Task 6 wiring — adjust if too noisy, but spec accepts a toast on every launch).

> If a toast on every launch reads as nagging in practice, the simplest fix is to have AppController suppress the success toast when the message is the literal "Patches already up to date". Adjust at Task 6 if this smoke step shows it's annoying.

- [ ] **Step 4: Verify staleness trigger**

```bash
touch -t 202401010000 "$RES/patches.zip.version"
```

Relaunch RetroNest. Expected: full refetch fires (`Patches updated to v<tag>`).

- [ ] **Step 5: Verify user-placed-file respect**

```bash
rm -f "$RES/patches.zip.version"
# patches.zip still present, mtime "now"
```

Relaunch. Expected: NO fetch fires (sidecar absent + mtime fresh = respect user). Log line absent or "no fetch needed".

```bash
touch -t 202401010000 "$RES/patches.zip"
```

Relaunch. Expected: fetch DOES fire (sidecar absent + mtime > 90 days).

- [ ] **Step 6: Verify offline behavior**

Disable network (Wi-Fi off, or `sudo ifconfig en0 down`):

```bash
rm -f "$RES/patches.zip" "$RES/patches.zip.version"
~/Documents/Projects/RetroNest-Project/cpp/build-arm64/RetroNest.app/Contents/MacOS/RetroNest 2>&1 \
  | grep -i "patches"
```

Expected: log warning `[Patches] Could not reach GitHub`. No toast (suppressed on startup-path failures). App starts cleanly; UI works.

Re-enable network, manually use the "Refresh PCSX2 Patches" button in settings. Expected: success toast.

- [ ] **Step 7: Verify Cmd+Q during fetch is clean**

With network re-enabled and resources wiped:

```bash
rm -f "$RES/patches.zip" "$RES/patches.zip.version"
~/Documents/Projects/RetroNest-Project/cpp/build-arm64/RetroNest.app/Contents/MacOS/RetroNest &
RNPID=$!
sleep 1  # in the middle of the fetch
kill -TERM $RNPID  # simulate Cmd+Q
wait $RNPID
echo "Exit: $?"
```

Expected: exit code 0 or 143 (SIGTERM). No 134 (SIGABRT). No `.tmp` leftover OR a `.tmp` that gets cleanly overwritten on next launch.

- [ ] **Step 8: Verify the knobs actually do something**

Launch RetroNest, open a PCSX2 game (DBZ TT2 NTSC is the original repro from the pickup memory; R&C 2 also fine). In PCSX2 settings:

- Set `pcsx2_enable_widescreen_patches=enabled`.

Launch the game. Expected: game renders 16:9 (not 4:3 pillarboxed). Confirm visually.

Also confirm PCSX2 logs no longer contain `Failed to open .../patches.zip: No such file`.

- [ ] **Step 9: Commit smoke notes if anything changed**

If steps 3 / 5 / 6 surfaced tweaks (e.g. suppress "already up to date" toast), commit those small fixes now under `fix(...)`.

- [ ] **Step 10: Update memory + final commit**

Add a completion entry to user memory (in a separate task outside the plan): update `MEMORY.md` to mark `patches-zip-followup` closed with a one-line reference to this implementation, mirroring the closure pattern for `sp7c-kickoff` and others.

---

## Acceptance criteria

- [ ] All three test binaries pass (`test_patches_sidecar`, `test_patches_installer`, plus existing `test_core_options` / `test_region_prefix` / `test_rcheevos_hash` still green).
- [ ] Fresh launch with no `patches.zip` produces the file + sidecar.
- [ ] PCSX2 warning `Failed to open .../patches.zip: No such file` no longer fires.
- [ ] Widescreen patches knob produces a visible 16:9 effect on at least one game.
- [ ] Cmd+Q during a fetch exits cleanly with no SIGABRT.
- [ ] Settings dialog "Patches" card exists and the refresh button works.
- [ ] No regressions in existing emulator-install flows (post-installer_utils extraction).
