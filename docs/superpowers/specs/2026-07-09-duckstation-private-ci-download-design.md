# DuckStation Private CI Build + Authenticated Download — Design

**Date:** 2026-07-09
**Status:** Approved (brainstorm), pending implementation plan

## Context

Every other libretro core RetroNest ships (pcsx2, ppsspp, dolphin) lives
in a **public** GitHub fork with a CI workflow that publishes releases;
RetroNest downloads them through `GitHubClient` + `EmulatorInstaller`
with **no authentication**.

DuckStation is different: its fork is **private** and must never be
publicly distributed (license). Today its manifest has `github_repo:
None`, so it is excluded from update checks and cannot be installed
through the app at all — it is built and deployed **only** by its local
`src/duckstation-libretro/package.sh`. That is why uninstalling it (or a
fresh machine) leaves no way to reinstall it from the UI.

This design makes DuckStation installable/updatable through the app like
the others, while keeping it private, by:
1. Adding a **private CI** to the DuckStation repo that publishes a
   (private) release.
2. Embedding a GitHub token at **build time** so RetroNest can
   authenticate downloads with **zero user friction** (no login, no
   token entry).
3. Teaching `GitHubClient`/`EmulatorInstaller` to authenticate downloads
   of private-repo releases.

## Goals

- DuckStation installs and updates through RetroNest's normal emulator
  UI, authenticated, with no user login or token entry.
- The token never appears in any committed file.
- No regression to the existing public-core download path.

## Non-Goals (deferred)

- **Public-build toggle.** No mechanism to exclude DuckStation (or the
  token) from a hypothetical public build. DuckStation stays in the
  codebase as-is; going public is a separate future concern.
- **Universal (arm64) CI build.** CI builds x86_64 only for now (see
  below). `package.sh` still builds universal locally when needed.
- **OAuth / device-flow / in-app token entry.** Not needed for a
  single-user, exclusive build.

## Key Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Auth mechanism | Build-time embedded fine-grained PAT via `dev_credentials.cmake` | Zero user friction; mirrors the existing ScreenScraper-credential pattern; never in the repo |
| CI runner | GitHub-hosted `macos-14` | Zero setup; 1–2 builds/month fits the budget |
| Build arches | **x86_64 only** | RetroNest's daily driver runs x86_64/Rosetta (some cores aren't arm-native yet); halves CI time/cost; flip to universal later by adding an arch job |
| CI trigger | Pushed version tag (`v*`) | Mirrors the pcsx2 workflow; matches the "test locally, then one build" rhythm |

**Budget note:** private-repo Actions bills macOS at **10×**, so the
2,000 included minutes ≈ **200 macOS wall-clock minutes/month**. An
x86_64-only DuckStation build (~20–30 min ≈ 200–300 billed) leaves room
for several builds/month; 1–2/month is comfortable. (The public
pcsx2/dolphin forks get unlimited minutes — only the private repo is
metered.)

## Section 1 — CI Workflow (DuckStation repo)

A tag-triggered `.github/workflows/libretro_release.yml`, mirroring
`pcsx2-libretro/.github/workflows/libretro_release.yml`. On a pushed
version tag:

1. **Build** `duckstation_libretro.dylib` for **x86_64** on `macos-14`,
   replicating `package.sh`'s cmake configuration.
2. **Package** the three deploy artifacts `package.sh` produces:
   - `dylibbundler` → `duckstation_libretro_libs/` (bundled non-system
     deps, `@loader_path`-rewritten, with the sibling-ref flattening the
     RetroNest installer also applies).
   - compile `metal_shaders.metallib` → `duckstation_libretro_resources/`
     (+ the `data/resources/` tree).
3. **Zip** all three into `duckstation_libretro.dylib.zip`, in the exact
   sibling layout `EmulatorInstaller` already expects:
   ```
   duckstation_libretro.dylib
   duckstation_libretro_libs/…
   duckstation_libretro_resources/…
   ```
4. **Publish** a **private** GitHub Release with that asset via
   `softprops/action-gh-release` (`permissions: contents: write`).

The `package.sh` logic is the source of truth for the build+package
steps; the workflow replicates it inline (same approach the pcsx2
workflow already takes).

## Section 2 — Embedded GitHub Token

Reuses the `dev_credentials.cmake` pattern already used for ScreenScraper
credentials.

1. `cpp/CMakeLists.txt`, next to the ScreenScraper cache vars:
   ```cmake
   set(RETRONEST_GITHUB_TOKEN "" CACHE STRING "GitHub PAT for private core downloads")
   ```
   Passed as a compile definition (e.g. `RETRONEST_GITHUB_TOKEN="…"`)
   only when non-empty.
2. Local, **gitignored** `dev_credentials.cmake` sets the real value.
   Verify `dev_credentials.cmake` is in `.gitignore` (belt-and-suspenders
   — it should be, since ScreenScraper secrets already live there).
3. RetroNest reads the compiled-in `#define`. Empty token → private
   downloads unavailable, no crash / no error spam.

**Token properties:** fine-grained PAT, **read-only, Contents, scoped to
just the DuckStation repo**, long expiry. Generated once, dropped into
`dev_credentials.cmake`, rebuild. If it ever leaked, blast radius is
read-only access to one private repo the owner already controls. Since
this DuckStation-enabled build is exclusive and never distributed, the
binary never leaves the owner's machine, so there is nothing to extract
the token from.

## Section 3 — RetroNest Download Wiring

**1. Manifest.** `manifests/duckstation.json` gains:
```json
"github_repo": "MarkrPearce96/duckstation-libretro",
"private": true
```
`private` becomes a field on the `Manifest` struct and is added to the
loader's known-keys list. This flips DuckStation from "excluded from
downloads" to "downloadable (authenticated)" and enrolls it in update
checks like the other cores.

**2. `GitHubClient` auth.** When a repo is private, attach
`Authorization: Bearer <compiled-in token>` to **both** the release-info
API query and the asset fetch. Empty token → skip private repos cleanly.

**3. Private asset download (the one real new detail).** Public cores
download via the asset's `browser_download_url` (a public CDN redirect,
no auth). That URL does **not** work for a private repo. The correct path
is the asset **API** endpoint:
```
GET /repos/{owner}/{repo}/releases/assets/{asset_id}
Accept: application/octet-stream
Authorization: Bearer <token>
```
GitHub responds with a redirect to a signed download URL (Qt's network
stack follows it). So for private repos the installer downloads via the
asset API endpoint + `octet-stream` header instead of
`browser_download_url`. Everything after the download — unzip into
`cores/`, `_libs`/`_resources` handling, sign/dequarantine/flatten — is
**unchanged**; it already works for the public cores.

## Testing

- **Unit-testable (stubbed manifest):**
  - Private manifest → `GitHubClient` selects the Bearer header; public →
    no auth header.
  - Private manifest → download uses the asset API endpoint +
    `octet-stream`; public → uses `browser_download_url`.
  - Empty token → private repo treated as unavailable (no crash).
- **End-to-end (manual, after first CI release):** install DuckStation
  through the app; confirm the core + `_libs` + `_resources` land in the
  cores dir and a PSX game boots with Metal rendering.

## Resolved Facts

- Private repo: `MarkrPearce96/duckstation-libretro` (origin;
  `upstream` = `stenzek/duckstation`).
- `cpp/dev_credentials.cmake` is already gitignored (`.gitignore:12`) and
  untracked — the token can go there safely.
- `package.sh` lives at
  `~/Documents/Projects/duckstation-libretro/src/duckstation-libretro/package.sh`
  and already produces the exact x86_64 dylib + `_libs/` + `_resources/`
  (metallib) artifacts the CI must replicate.

## Open Items to Resolve in the Plan

- Decide whether the `private` update-check path should surface a
  distinct "update available (private)" state or reuse the existing one
  (lean: reuse).
