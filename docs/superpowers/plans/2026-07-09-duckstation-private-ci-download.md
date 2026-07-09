# DuckStation Private CI Build + Authenticated Download — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make DuckStation installable/updatable through RetroNest's normal emulator UI by authenticating downloads of its private GitHub release, with a build-time embedded token requiring zero user interaction, plus a private CI workflow that publishes the release.

**Architecture:** A tag-triggered GitHub Actions workflow in the private `MarkrPearce96/duckstation-libretro` repo builds + packages an x86_64 core and publishes a private release (Section 1). RetroNest gains a `private` manifest flag; when set, the installer sends `Authorization: Bearer <compiled-in token>` and downloads the asset via the GitHub *release-assets API* endpoint (private assets can't use `browser_download_url`). The token is compiled in via the existing `dev_credentials.cmake` pattern (Sections 2–3).

**Tech Stack:** C++17 / Qt6 (QNetworkAccessManager), CMake, GitHub Actions (macos-14), QtTest.

## Global Constraints

- CI builds **x86_64 only** (RetroNest daily driver is x86_64/Rosetta). Runner: `macos-14`. Trigger: pushed version tag (`v*`).
- Private repo: `MarkrPearce96/duckstation-libretro` (`upstream` = `stenzek/duckstation`).
- Token: fine-grained PAT, read-only **Contents** scope, **that repo only**, long expiry. Lives ONLY in gitignored `cpp/dev_credentials.cmake` (already gitignored: `.gitignore:12`) — never in any committed file.
- The compiled-in token define must default to empty and compile cleanly when absent (public/CI builds without `dev_credentials.cmake`).
- No regression to the existing public-core download path (pcsx2/ppsspp/dolphin download via `browser_download_url`, no auth — unchanged).
- Build x86_64 with `arch -x86_64 /usr/local/bin/cmake` against `cpp/build-x86_64`. Run the suite with `arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64`.

---

## File Structure

- `cpp/src/core/manifest.h` — add `bool is_private` field to `EmulatorManifest`.
- `cpp/src/core/manifest_loader.cpp` — parse `"private"`; add `"private"` to `kKnownKeys`.
- `cpp/src/services/github_credentials.h` / `.cpp` — **new**, app-side; `GitHubCredentials::token()` returns the compiled-in PAT (or empty). Mirrors `scraper_credentials`.
- `cpp/src/services/installer_utils.h` / `.cpp` — `httpGet` gains an optional `authToken` param (Bearer header when set).
- `cpp/src/services/emulator_installer.cpp` — for private manifests: auth the release-info fetch, capture the asset **API url**, and download via the asset API endpoint + `octet-stream` + Bearer.
- `cpp/CMakeLists.txt` — `RETRONEST_GITHUB_TOKEN` cache var + compile definition on the app target; add `github_credentials.cpp` to app SOURCES + the `test_emulator_version_records` target.
- `manifests/duckstation.json` — add `github_repo` + `"private": true`.
- `.github/workflows/libretro_release.yml` **in the DuckStation repo** (not RetroNest) — the CI.
- Tests: `cpp/tests/test_manifest_libretro_fields.cpp` (private flag), `cpp/tests/test_github_download_auth.cpp` (**new** — token accessor + asset-url selection).

---

### Task 1: Manifest `private` flag

**Files:**
- Modify: `cpp/src/core/manifest.h` (struct `EmulatorManifest`, near `bool has_patches`)
- Modify: `cpp/src/core/manifest_loader.cpp:62-67` (kKnownKeys) and `:85` (parse)
- Test: `cpp/tests/test_manifest_libretro_fields.cpp`

**Interfaces:**
- Produces: `EmulatorManifest::is_private` (bool, default `false`).

- [ ] **Step 1: Write the failing test**

Add to `cpp/tests/test_manifest_libretro_fields.cpp` (inside the test class, new slot; mirror an existing case's file-writing helper if present, else write the manifest inline to a `QTemporaryDir`):

```cpp
void privateFlagParses() {
    QTemporaryDir dir;
    QFile f(dir.path() + "/priv.json");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(R"({
        "manifest_version": 1, "id": "priv", "name": "Priv",
        "systems": ["psx"], "backend": "libretro",
        "core_dylib": "priv_libretro.dylib",
        "github_repo": "owner/priv", "private": true
    })");
    f.close();

    ManifestLoader loader;
    QVERIFY(loader.loadAll(dir.path()));
    const EmulatorManifest* m = loader.emulatorById("priv");
    QVERIFY(m != nullptr);
    QVERIFY(m->is_private);

    // Default is false when the key is absent.
    QFile f2(dir.path() + "/pub.json");
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write(R"({"manifest_version":1,"id":"pub","name":"Pub","systems":["psx"],
                 "backend":"libretro","core_dylib":"pub_libretro.dylib",
                 "github_repo":"owner/pub"})");
    f2.close();
    ManifestLoader loader2;
    QVERIFY(loader2.loadAll(dir.path()));
    QVERIFY(!loader2.emulatorById("pub")->is_private);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_manifest_libretro_fields -j 6 && cpp/build-x86_64/test_manifest_libretro_fields`
Expected: FAIL — `is_private` is not a member of `EmulatorManifest` (compile error).

- [ ] **Step 3: Add the field**

In `cpp/src/core/manifest.h`, in `struct EmulatorManifest`, right after `bool has_patches = false;`:

```cpp
    bool is_private = false;        // private repo → downloads need a Bearer token
```

- [ ] **Step 4: Parse it + register the key**

In `cpp/src/core/manifest_loader.cpp`, add `"private"` to `kKnownKeys` (line 66, after `"detail_page"`):

```cpp
            "core_arch", "logo", "detail_page", "private",
```

And after the `m.github_repo = obj["github_repo"].toString();` line (~85):

```cpp
        m.is_private = obj.value("private").toBool(false);
```

- [ ] **Step 5: Run test to verify it passes**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_manifest_libretro_fields -j 6 && cpp/build-x86_64/test_manifest_libretro_fields`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add cpp/src/core/manifest.h cpp/src/core/manifest_loader.cpp cpp/tests/test_manifest_libretro_fields.cpp
git commit -m "feat: manifest 'private' flag for auth-gated core downloads"
```

---

### Task 2: Compiled-in GitHub token accessor

**Files:**
- Create: `cpp/src/services/github_credentials.h`, `cpp/src/services/github_credentials.cpp`
- Modify: `cpp/CMakeLists.txt` (cache var ~line 55; compile def ~line 340; app SOURCES ~line 235; test target ~line 612)
- Test: `cpp/tests/test_github_download_auth.cpp` (**new**)

**Interfaces:**
- Produces: `QString GitHubCredentials::token();` — the compiled-in PAT, or `""` when the define is absent/empty.

- [ ] **Step 1: Write the failing test**

Create `cpp/tests/test_github_download_auth.cpp`:

```cpp
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
```

- [ ] **Step 2: Create the accessor**

`cpp/src/services/github_credentials.h`:

```cpp
#pragma once
#include <QString>

// The GitHub Personal Access Token compiled into private (dev) builds via
// dev_credentials.cmake → RETRONEST_GITHUB_TOKEN. Empty in any build without
// that gitignored config (e.g. a public build), which disables private-repo
// downloads cleanly. Mirrors ScraperCredentials' build-time secret pattern.
namespace GitHubCredentials {
QString token();
}
```

`cpp/src/services/github_credentials.cpp`:

```cpp
#include "github_credentials.h"

namespace GitHubCredentials {
QString token() {
#ifdef RETRONEST_GITHUB_TOKEN
    return QStringLiteral(RETRONEST_GITHUB_TOKEN);
#else
    return {};
#endif
}
}
```

- [ ] **Step 3: Wire CMake — cache var + compile def + sources**

In `cpp/CMakeLists.txt`, after the `SCREENSCRAPER_SOFTNAME` cache var (~line 56):

```cmake
set(RETRONEST_GITHUB_TOKEN "" CACHE STRING "GitHub PAT for private core downloads")
```

In the app-target `target_compile_definitions(${PROJECT_NAME} PRIVATE ...)` block (~line 340), add:

```cmake
    RETRONEST_GITHUB_TOKEN="${RETRONEST_GITHUB_TOKEN}"
```

Add to the app `set(SOURCES ...)` list (near `scraper_credentials.cpp`, ~line 235):

```cmake
    src/services/github_credentials.cpp
```

The `#ifdef` fallback means test targets need NO define — they just link the TU. `github_credentials.cpp` is app-side (not in `retronest_core`), so the test compiles it directly; `chooseAssetDownloadUrl` (Task 3) lives in `installer_utils.cpp` which IS in `retronest_core`, so it comes via the link. Add the new test executable (after `test_emulator_version_records`, ~line 618):

```cmake
add_executable(test_github_download_auth
    tests/test_github_download_auth.cpp
    src/services/github_credentials.cpp
)
target_link_libraries(test_github_download_auth PRIVATE retronest_core Qt6::Test)
add_test(NAME GitHubDownloadAuth COMMAND test_github_download_auth)
```

- [ ] **Step 4: Reconfigure, build, run**

Run: `arch -x86_64 /usr/local/bin/cmake -S cpp -B cpp/build-x86_64 -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2" && arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_github_download_auth -j 6 && cpp/build-x86_64/test_github_download_auth`
Expected: PASS (`tokenEmptyByDefault`).

- [ ] **Step 5: Commit**

```bash
git add cpp/src/services/github_credentials.h cpp/src/services/github_credentials.cpp cpp/CMakeLists.txt cpp/tests/test_github_download_auth.cpp
git commit -m "feat: build-time GitHub token accessor (dev_credentials pattern)"
```

---

### Task 3: Authenticated release-info fetch + capture asset API url

**Files:**
- Modify: `cpp/src/services/installer_utils.h`, `cpp/src/services/installer_utils.cpp` (`httpGet`)
- Modify: `cpp/src/services/emulator_installer.cpp` (release resolution ~line 160-208; add a pure helper `chooseAssetDownloadUrl`)
- Test: `cpp/tests/test_github_download_auth.cpp`

**Interfaces:**
- Consumes: `GitHubCredentials::token()` (Task 2), `EmulatorManifest::is_private` (Task 1).
- Produces (free function, declared in `emulator_installer.cpp`'s anonymous namespace is NOT testable; declare it in a small header so the test can call it):
  - Add to `cpp/src/services/installer_utils.h`:
    `QString chooseAssetDownloadUrl(const QJsonObject& asset, bool isPrivate);`
  - Change `InstallerUtils::httpGet` signature to:
    `QByteArray httpGet(const QString& url, int timeoutMs = 30000, const QString& context = {}, const QString& authToken = {});`

- [ ] **Step 1: Write the failing test**

Add to `cpp/tests/test_github_download_auth.cpp` (new slots + include):

```cpp
#include "services/installer_utils.h"
#include <QJsonObject>
```

```cpp
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
```

- [ ] **Step 2: Run test to verify it fails**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_github_download_auth -j 6`
Expected: FAIL — `chooseAssetDownloadUrl` undeclared (compile error).

- [ ] **Step 3: Add the helper + auth param**

In `cpp/src/services/installer_utils.h`, add to the `InstallerUtils` namespace:

```cpp
#include <QJsonObject>
// ...
QByteArray httpGet(const QString& url, int timeoutMs = 30000,
                   const QString& context = {}, const QString& authToken = {});

// Which URL to download a release asset from: the public CDN
// (browser_download_url) for public repos, or the release-assets API
// endpoint (asset["url"]) for private repos, which authenticates.
QString chooseAssetDownloadUrl(const QJsonObject& asset, bool isPrivate);
```

In `cpp/src/services/installer_utils.cpp`, update `httpGet` to add the header when `authToken` is non-empty (right after the `Accept` header):

```cpp
QByteArray httpGet(const QString& url, int timeoutMs, const QString& context,
                   const QString& authToken) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    req.setRawHeader("Accept", "application/json");
    if (!authToken.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // ... rest unchanged ...
```

And add the helper at the end of the namespace:

```cpp
QString chooseAssetDownloadUrl(const QJsonObject& asset, bool isPrivate) {
    return isPrivate ? asset.value("url").toString()
                     : asset.value("browser_download_url").toString();
}
```

- [ ] **Step 4: Use them in the installer's release resolution**

In `cpp/src/services/emulator_installer.cpp`, in the release-resolution function:

Auth the release-info fetch (replace line ~161):

```cpp
    const QString authToken =
        manifest.is_private ? GitHubCredentials::token() : QString();
    if (manifest.is_private && authToken.isEmpty()) {
        info.errorMessage = manifest.name +
            " requires a GitHub token (private build) — not available in this build.";
        return info;
    }
    QByteArray releaseJson = InstallerUtils::httpGet(apiUrl, 30000, "[Installer]", authToken);
```

Capture the asset API url alongside browser url (in the asset loop, ~186-195): change `assetUrls` to store the chosen url:

```cpp
    for (const auto& a : assets) {
        QJsonObject asset = a.toObject();
        QString name = asset["name"].toString();
        QString url = InstallerUtils::chooseAssetDownloadUrl(asset, manifest.is_private);
        QString digest = asset["digest"].toString();
        assetNames.append(name);
        assetUrls.insert(name, url);
        if (digest.startsWith("sha256:", Qt::CaseInsensitive))
            assetDigests.insert(name, digest.mid(7).toLower());
    }
```

Add the include at the top of `emulator_installer.cpp`:

```cpp
#include "github_credentials.h"
```

- [ ] **Step 5: Run test to verify it passes**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 --target test_github_download_auth -j 6 && cpp/build-x86_64/test_github_download_auth`
Expected: PASS (both slots).

- [ ] **Step 6: Commit**

```bash
git add cpp/src/services/installer_utils.h cpp/src/services/installer_utils.cpp cpp/src/services/emulator_installer.cpp cpp/tests/test_github_download_auth.cpp
git commit -m "feat: authenticate private release-info fetch + select asset API url"
```

---

### Task 4: Authenticated asset download — sync + async paths

**Files:**
- Modify: `cpp/src/services/emulator_installer.cpp` — `downloadSync` (~line 29), sync call site (~line 395), async `apiReq` (~line 447), async asset loop (~line 485), `startDirectDownload` (~line 518) + `dlReq` (~line 537)

**Interfaces:**
- Consumes: `manifest.is_private` (Task 1), `GitHubCredentials::token()` (Task 2), `InstallerUtils::chooseAssetDownloadUrl` (Task 3).
- Convention used throughout: a **non-empty `authToken` means "private"** — it is set only for private manifests, so `!authToken.isEmpty()` is the private signal wherever the manifest isn't in scope.

The asset API endpoint returns the binary only with `Accept:
application/octet-stream` + `Authorization: Bearer`. There are TWO
download paths — the synchronous CLI path (`downloadSync`) and the async
GUI path (`startDirectDownload`/`dlReq`) — plus the async path builds its
own `/releases/latest` request (`apiReq`) inline instead of via
`InstallerUtils::httpGet`. All must be handled. Verified by the
end-to-end install (Task 6) — the inline network I/O has no unit seam.

- [ ] **Step 1: Sync path — `downloadSync` + its call site**

Change `downloadSync` (~line 29):

```cpp
static bool downloadSync(const QString& url, const QString& destPath,
                         const QString& authToken = {}) {
    // ... existing setup ...
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "RetroNest/1.0");
    if (!authToken.isEmpty()) {
        req.setRawHeader("Accept", "application/octet-stream");
        req.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());
    }
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // ... rest unchanged ...
```

At the `downloadSync` call site in `installSync` (~line 395), compute + pass the token:

```cpp
    const QString authToken =
        manifest.is_private ? GitHubCredentials::token() : QString();
    if (!downloadSync(release.downloadUrl, tempFile, authToken)) {
```

- [ ] **Step 2: Async path — auth `apiReq`, choose asset url, thread token to `startDirectDownload`**

In `installEmulatorAsync`, right after `const QString emuId = manifest.id;` (~line 444), capture the token so it's available to the lambda:

```cpp
    const QString authToken =
        manifest.is_private ? GitHubCredentials::token() : QString();
```

Auth the JSON request `apiReq` (~line 449, after the `Accept` header):

```cpp
    if (!authToken.isEmpty())
        apiReq.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());
```

Add `authToken` to the lambda capture list (~line 454): change
`[this, apiReply, nam, emuId, installPath]` to
`[this, apiReply, nam, emuId, installPath, authToken]`.

In the async asset loop, replace the `browser_download_url` line (~line 485):

```cpp
                    QString url = InstallerUtils::chooseAssetDownloadUrl(asset, !authToken.isEmpty());
```

Pass the token to `startDirectDownload` (~line 509):

```cpp
                startDirectDownload(assetName, downloadUrl, tagName, publishedAt,
                                    sha256, installPath, authToken);
```

- [ ] **Step 3: `startDirectDownload` — add param + auth `dlReq`**

Add an `authToken` parameter (default empty so the adapter-direct caller is
unaffected) to both the declaration in `emulator_installer.h` and the
definition (~line 518):

```cpp
void EmulatorInstaller::startDirectDownload(const QString& assetName,
                                              const QString& downloadUrl,
                                              const QString& tagName,
                                              const QString& publishedAt,
                                              const QString& sha256,
                                              const QString& installPath,
                                              const QString& authToken) {
```

After `dlReq.setHeader(QNetworkRequest::UserAgentHeader, ...)` (~line 538):

```cpp
    if (!authToken.isEmpty()) {
        dlReq.setRawHeader("Accept", "application/octet-stream");
        dlReq.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());
    }
```

(The adapter-direct call site of `startDirectDownload` keeps its existing
argument list — the new `authToken` defaults to empty, i.e. public.)

Add the include if not already present (from Task 3): `#include "github_credentials.h"`.

- [ ] **Step 4: Build the app + run the full suite (no regression)**

Run: `arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 6 2>&1 | grep -iE "error:|Built target RetroNest$" | tail -2 && arch -x86_64 /usr/local/bin/ctest --test-dir cpp/build-x86_64 --output-on-failure | tail -3`
Expected: app builds; 100% tests pass (public download path unchanged since `is_private` defaults false).

- [ ] **Step 4: Commit**

```bash
git add cpp/src/services/emulator_installer.cpp
git commit -m "feat: authenticated octet-stream download for private release assets"
```

---

### Task 5: CI workflow in the DuckStation repo

**Files:**
- Create (in `~/Documents/Projects/duckstation-libretro`, **NOT the RetroNest repo**): `.github/workflows/libretro_release.yml`

**Interfaces:** none (separate repo). Verified by a real CI run (Task 6).

- [ ] **Step 1: Read the reference workflow + package.sh**

Read `~/Documents/Projects/pcsx2-libretro/.github/workflows/libretro_release.yml` (build/bundle/release job shape) and `~/Documents/Projects/duckstation-libretro/src/duckstation-libretro/package.sh` (the exact x86_64 cmake config, dylibbundler step, metallib compile, and the `_libs`/`_resources` layout).

- [ ] **Step 2: Author the workflow**

Create `.github/workflows/libretro_release.yml` in the DuckStation repo. It must:
- Trigger: `on: push: tags: ['v*']`; `permissions: contents: write`.
- `build` job on `runs-on: macos-14`:
  - checkout, install build deps (brew), configure + build `duckstation_libretro` for **x86_64** using the cmake invocation from `package.sh` (x86_64 slice only — do NOT build arm64).
  - Run the `dylibbundler` + `install_name_tool` flatten steps and the `metal_shaders.metallib` compile exactly as `package.sh` does, producing `duckstation_libretro.dylib`, `duckstation_libretro_libs/`, `duckstation_libretro_resources/`.
  - `zip -r duckstation_libretro.dylib.zip duckstation_libretro.dylib duckstation_libretro_libs duckstation_libretro_resources` (flat layout at zip root — the RetroNest installer unzips into `cores/` and expects these as siblings).
  - upload the zip as an artifact.
- `release` job: download the artifact, publish via `softprops/action-gh-release@v2` with the tag, attaching `duckstation_libretro.dylib.zip`. Because the repo is private, the release is private automatically.

(No RetroNest-side test — this is validated by pushing a tag and inspecting the CI run + published asset in Task 6.)

- [ ] **Step 3: Commit (in the DuckStation repo)**

```bash
cd ~/Documents/Projects/duckstation-libretro
git add .github/workflows/libretro_release.yml
git commit -m "ci: x86_64 libretro release workflow (private, tag-triggered)"
git push origin <branch>
```

---

### Task 6: Wire the manifest + end-to-end verification

**Files:**
- Modify: `manifests/duckstation.json`

- [ ] **Step 1: Generate + install the token**

On GitHub, create a fine-grained PAT: **read-only Contents, `MarkrPearce96/duckstation-libretro` only**, long expiry. Add to `cpp/dev_credentials.cmake` (gitignored):

```cmake
set(RETRONEST_GITHUB_TOKEN "github_pat_xxxxxxxx")
```

- [ ] **Step 2: Add github_repo + private to the manifest**

In `manifests/duckstation.json`, add:

```json
"github_repo": "MarkrPearce96/duckstation-libretro",
"private": true,
```

(Keep `core_dylib`, `install_folder`, etc. as-is. Confirm the CI asset name `duckstation_libretro.dylib.zip` is selected by the adapter's asset matching — LibretroAdapter's default keys off the `.dylib.zip` suffix.)

- [ ] **Step 3: Cut the first private release**

In the DuckStation repo, push a version tag (e.g. `git tag v2026.07.09 && git push origin v2026.07.09`). Wait for the CI to build + publish. Confirm the release + `duckstation_libretro.dylib.zip` asset exist (private).

- [ ] **Step 4: Reconfigure + build RetroNest with the token**

Run: `arch -x86_64 /usr/local/bin/cmake -S cpp -B cpp/build-x86_64 -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2" && arch -x86_64 /usr/local/bin/cmake --build cpp/build-x86_64 -j 6`
Expected: builds; the token is now compiled in.

- [ ] **Step 5: End-to-end install through the app**

Launch the app → Settings → Emulators → **uninstall DuckStation** (if present) → **Install**. Watch the log: `[Installer] Async installing "DuckStation"...`, SHA256 verify, `Libretro core installed`. Confirm `cores/duckstation_libretro.dylib` + `_libs/` + `_resources/` (with `metal_shaders.metallib`) landed, then boot a PSX game (Metal rendering).

- [ ] **Step 6: Commit (RetroNest repo — manifest only; NOT dev_credentials.cmake)**

```bash
git add manifests/duckstation.json
git status   # verify cpp/dev_credentials.cmake is NOT staged (it's gitignored)
git commit -m "feat: DuckStation installable via authenticated private release"
git push origin main
```

---

## Self-Review Notes

- **Spec coverage:** Section 1 → Task 5; Section 2 → Task 2 (+ Task 6 token gen); Section 3 → Tasks 1, 3, 4, 6. Testing section → Tasks 1–4 (unit) + Task 6 (e2e). ✓
- **Private-asset endpoint** (the spec's one real detail) → Task 3 (`chooseAssetDownloadUrl`) + Task 4 (octet-stream + Bearer). ✓
- **No-token cleanliness** → Task 2 `#ifdef` fallback + Task 3 empty-token guard. ✓
- **Open item (reuse existing update-check state):** resolved — no new state; `is_private` only changes auth, not UX. The manifest gaining `github_repo` enrolls DuckStation in the existing update-check flow automatically.
