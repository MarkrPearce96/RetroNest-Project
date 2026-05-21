# pcsx2-libretro GitHub publishing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish the pcsx2-libretro fork to GitHub with tag-triggered CI that builds the macOS x86_64 core and packages it as a downloadable Release asset RetroNest can consume.

**Architecture:** Push the existing fork (with full upstream PCSX2 history) to `origin` on GitHub. Add a single GHA workflow that triggers on `v*` tags, builds the libretro target on `macos-13`, zips the dylib + resources + a `VERSION` file, and attaches it to an auto-created Release. Document the rebase + release process in `UPSTREAM-UPDATE.md` and add a small fork-notice block to the existing upstream `README.md` so the GitHub landing page identifies the fork.

**Tech Stack:** GitHub Actions, `softprops/action-gh-release@v2`, Homebrew (macos-13 runner), cmake, lipo (not used — single-arch), zip, git.

**Spec:** `/Users/mark/Documents/Projects/RetroNest-Project/docs/superpowers/specs/2026-05-21-pcsx2-libretro-github-publishing-design.md`

**Working directory for all pcsx2-libretro tasks:** `/Users/mark/Documents/Projects/pcsx2-libretro`

---

## File Structure

Three new/modified files in the pcsx2-libretro repo:

| File | Action | Responsibility |
|---|---|---|
| `.github/workflows/libretro_release.yml` | Create | Tag-triggered build + release pipeline |
| `UPSTREAM-UPDATE.md` | Create | Rebase + release process documentation |
| `README.md` | Modify (prepend small fork-notice block) | Fork identity on GitHub landing page; links to UPSTREAM-UPDATE.md and RetroNest. All upstream PCSX2 README content below stays intact. |

The fork-notice prepend is the only modification to an upstream-tracked file. It's append-at-top so rebase conflicts are minimal (line-1 additions are the lowest-conflict modification possible).

No source code changes. No changes to `pcsx2-libretro/`, `bin/resources/`, or any other PCSX2 source.

---

## Task 1: Pre-flight verification

**Files:** None modified. Verification only.

- [ ] **Step 1: Verify clean working tree**

Run: `git -C /Users/mark/Documents/Projects/pcsx2-libretro status --short`
Expected: empty output (no uncommitted changes).

If output is non-empty, STOP and resolve with the user before proceeding — uncommitted changes belong to other work and shouldn't be bundled with this ship.

- [ ] **Step 2: Verify branch is `main`**

Run: `git -C /Users/mark/Documents/Projects/pcsx2-libretro rev-parse --abbrev-ref HEAD`
Expected: `main`

- [ ] **Step 3: Verify upstream remote points at PCSX2/pcsx2**

Run: `git -C /Users/mark/Documents/Projects/pcsx2-libretro remote -v`
Expected output contains:
```
upstream	https://github.com/PCSX2/pcsx2.git (fetch)
upstream	https://github.com/PCSX2/pcsx2.git (push)
```
And does NOT yet contain a line starting with `origin`. (If `origin` already exists, ask the user before continuing — it likely points somewhere we don't want to overwrite.)

- [ ] **Step 4: Verify resources directory exists at expected path**

Run: `ls /Users/mark/Documents/Projects/pcsx2-libretro/bin/resources/ | head -5`
Expected: at least `GameIndex.yaml`, `RedumpDatabase.yaml`, `default.metallib`, `fonts`, `game_controller_db.txt` listed.

This confirms the workflow's resources-zip step will find the dir at the path the spec assumed.

- [ ] **Step 5: Verify no top-level `README.fork.md` or `UPSTREAM-UPDATE.md` already exists**

Run: `ls /Users/mark/Documents/Projects/pcsx2-libretro/UPSTREAM-UPDATE.md /Users/mark/Documents/Projects/pcsx2-libretro/.github/workflows/libretro_release.yml 2>&1`
Expected: both `No such file or directory` errors. (If either exists, STOP and ask the user — there may be prior work that conflicts.)

---

## Task 2: Write the GHA workflow

**Files:**
- Create: `/Users/mark/Documents/Projects/pcsx2-libretro/.github/workflows/libretro_release.yml`

- [ ] **Step 1: Create the workflow file**

Write this exact content to `/Users/mark/Documents/Projects/pcsx2-libretro/.github/workflows/libretro_release.yml`:

```yaml
name: libretro_release

# Builds the pcsx2-libretro core on tag push and publishes a GitHub Release
# with the macOS x86_64 dylib + resources packaged as a single zip.
#
# Spec: see RetroNest-Project/docs/superpowers/specs/2026-05-21-pcsx2-libretro-github-publishing-design.md
# Process: see UPSTREAM-UPDATE.md at the repo root.

on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write  # required for softprops/action-gh-release to create releases

jobs:
  build:
    name: Build macOS x86_64
    # macos-13 = Intel runner. When GitHub retires macos-13, switch to macos-14
    # with Rosetta + the x86_64 brew dance documented in UPSTREAM-UPDATE.md.
    runs-on: macos-13
    steps:
      - name: Checkout
        uses: actions/checkout@v4
        with:
          submodules: recursive
          fetch-depth: 0  # full history so the release job can read SHAs

      - name: Install Homebrew dependencies
        run: |
          brew update
          brew install \
            cmake \
            ninja \
            libpng \
            libjpeg \
            libzip \
            zstd \
            qt@6 \
            sdl3 \
            shaderc \
            rapidyaml \
            plutosvg \
            plutovg \
            molten-vk \
            lz4

      - name: Configure (cmake)
        run: |
          cmake -S . -B build \
            -DENABLE_LIBRETRO=ON \
            -DENABLE_QT_UI=OFF \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_OSX_ARCHITECTURES=x86_64

      - name: Build pcsx2_libretro target
        run: cmake --build build --target pcsx2_libretro -j

      - name: Upload dylib artifact
        uses: actions/upload-artifact@v4
        with:
          name: pcsx2_libretro-dylib
          path: build/pcsx2-libretro/pcsx2_libretro.dylib
          if-no-files-found: error

      - name: Upload resources artifact
        uses: actions/upload-artifact@v4
        with:
          name: pcsx2_libretro-resources
          path: bin/resources/
          if-no-files-found: error

  release:
    name: Package and publish release
    needs: build
    runs-on: ubuntu-latest
    steps:
      - name: Checkout (for SHA lookup)
        uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Download dylib artifact
        uses: actions/download-artifact@v4
        with:
          name: pcsx2_libretro-dylib
          path: stage/

      - name: Download resources artifact
        uses: actions/download-artifact@v4
        with:
          name: pcsx2_libretro-resources
          path: stage/pcsx2_libretro_resources/

      - name: Fetch upstream master (for SHA lookup)
        run: |
          git remote add upstream https://github.com/PCSX2/pcsx2.git
          git fetch upstream master --depth=200

      - name: Compute version metadata
        id: meta
        run: |
          TAG="${GITHUB_REF#refs/tags/}"
          FORK_SHA="$(git rev-parse HEAD)"
          # upstream_sha: the merge-base of HEAD and upstream/master is the
          # upstream tip we last rebased onto (after a clean rebase, all of
          # upstream/master is reachable from HEAD, so merge-base returns
          # upstream/master's tip).
          UPSTREAM_SHA="$(git merge-base HEAD upstream/master)"
          UPSTREAM_SHORT="$(echo "$UPSTREAM_SHA" | cut -c1-7)"
          BUILD_DATE="$(date -u +'%Y-%m-%dT%H:%M:%SZ')"
          {
            echo "tag=$TAG"
            echo "platform=macos-x86_64"
            echo "fork_sha=$FORK_SHA"
            echo "upstream_sha=$UPSTREAM_SHA"
            echo "upstream_ref=upstream/master"
            echo "build_date=$BUILD_DATE"
          } > stage/VERSION
          echo "tag=$TAG" >> "$GITHUB_OUTPUT"
          echo "upstream_short=$UPSTREAM_SHORT" >> "$GITHUB_OUTPUT"

      - name: Verify staged contents
        run: |
          ls -la stage/
          test -f stage/pcsx2_libretro.dylib
          test -d stage/pcsx2_libretro_resources
          test -f stage/VERSION
          cat stage/VERSION

      - name: Create zip
        run: |
          cd stage
          zip -r ../pcsx2_libretro-macos-x86_64.zip \
            pcsx2_libretro.dylib \
            pcsx2_libretro_resources \
            VERSION
          cd ..
          ls -la pcsx2_libretro-macos-x86_64.zip

      - name: Compose release body
        id: body
        run: |
          {
            echo "**Platform:** macOS x86_64 (Intel + Apple Silicon via Rosetta)"
            echo "**Upstream PCSX2:** ${{ steps.meta.outputs.upstream_short }} (master)"
            echo ""
            echo "## Changes since previous release"
          } > release_body.md

      - name: Publish release
        uses: softprops/action-gh-release@v2
        with:
          tag_name: ${{ steps.meta.outputs.tag }}
          name: ${{ steps.meta.outputs.tag }}
          body_path: release_body.md
          generate_release_notes: true   # appends auto-generated commit list below body
          files: pcsx2_libretro-macos-x86_64.zip
          fail_on_unmatched_files: true
```

- [ ] **Step 2: YAML syntax sanity check**

Run: `python3 -c "import yaml; yaml.safe_load(open('/Users/mark/Documents/Projects/pcsx2-libretro/.github/workflows/libretro_release.yml'))" && echo OK`
Expected: `OK` (no exception).

If this fails, the YAML has a syntax error — fix indentation/quoting and re-run.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add .github/workflows/libretro_release.yml
git commit -m "$(cat <<'EOF'
ci: add libretro_release workflow

Tag-triggered (v*) GHA workflow that builds pcsx2_libretro on macos-13,
zips the dylib + bin/resources/ + a VERSION metadata file into
pcsx2_libretro-macos-x86_64.zip, and publishes it as a GitHub Release
asset. RetroNest consumes the release per the SP-publish spec.

Spec: RetroNest-Project/docs/superpowers/specs/2026-05-21-pcsx2-libretro-github-publishing-design.md

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Write UPSTREAM-UPDATE.md

**Files:**
- Create: `/Users/mark/Documents/Projects/pcsx2-libretro/UPSTREAM-UPDATE.md`

- [ ] **Step 1: Write the doc**

Write this exact content to `/Users/mark/Documents/Projects/pcsx2-libretro/UPSTREAM-UPDATE.md`:

````markdown
# Updating from upstream PCSX2

This fork rebases onto `PCSX2/pcsx2` master. All RetroNest-specific code
lives in `pcsx2-libretro/` (sibling of `pcsx2-qt/` and `pcsx2-gsrunner/`).
There is exactly one modification to a top-level upstream file: a 4-line
block in `CMakeLists.txt` that conditionally includes `pcsx2-libretro/`.
This is the only intentional source of rebase friction.

## Rebase + release process

```sh
# 1. Fetch upstream
git fetch upstream

# 2. Rebase onto upstream master
git checkout main
git rebase upstream/master

# 3. Resolve conflicts. Typical: 0–1 anchor lines in top-level CMakeLists.txt
#    (~11/12 months trivial, 30s fix). The fork-notice block at the top of
#    README.md may also drift if upstream restructures their README.

# 4. Local sanity build
cmake -S . -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pcsx2_libretro -j

# 5. (Optional) local smoke test before publishing
cp build/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
rsync -a --delete bin/resources/ \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/

# 6. Push the rebased main to your fork
git push origin main --force-with-lease

# 7. Cut a release. The GitHub Actions workflow does the rest.
TAG="v$(date -u +%Y.%m.%d)"
git tag "$TAG"
git push origin "$TAG"
```

That last `git push origin <tag>` triggers `.github/workflows/libretro_release.yml`.
The workflow builds the core on a `macos-13` runner, packages
`pcsx2_libretro-macos-x86_64.zip`, and creates a GitHub Release with the
zip attached. RetroNest can then download it.

If you need to cut a second release on the same day, suffix the tag with
`.1`, `.2`, etc — e.g. `v2026.05.21.1`.

## Discipline: keep the rebase surface tiny

Never edit upstream files outside the single 4-line block in top-level
`CMakeLists.txt` and the small fork-notice block at the top of `README.md`.
All RetroNest-specific code lives in `pcsx2-libretro/` (a new sibling
directory, not an edit to upstream). There are two narrow exceptions
already in place for libretro-specific dispatch tables (audio backend +
input source enums), each comment-flagged `// pcsx2-libretro… (SPN)` for
future rebase reviewers — do not widen these.

## What can go wrong on rebase

Three patterns, in rough order of frequency:

1. **CMakeLists.txt anchor-line conflict** (~11/12 months trivial). The
   conditional `add_subdirectory(pcsx2-libretro)` block has an anchor line
   above it that occasionally drifts when upstream restructures
   directories. 30 seconds to manually re-align.

2. **PCSX2 internal API drift** (2–4 times per year). An upstream commit
   renames a function or restructures a class the libretro shim depends
   on. The build fails inside `pcsx2-libretro/` with a clear error
   message. Fix is minutes-to-hour inside the shim — never propagate the
   change upward.

3. **New upstream Homebrew dep**. The cmake configure step in CI fails
   with a missing-package error. Edit `.github/workflows/libretro_release.yml`'s
   `Install Homebrew dependencies` step, add the new brew package, commit,
   re-cut the tag.

## What can go wrong in CI

The workflow logs live at `https://github.com/<your-user>/pcsx2-libretro/actions`.

**Most common failure:** missing brew dep. The fix is above.

**Less common: macos-13 runner retirement.** GitHub will eventually retire
the Intel `macos-13` runner image. When that happens:

- Change `runs-on: macos-13` to `runs-on: macos-14`
- Wrap the cmake configure + build steps in an `arch -x86_64` invocation
- Set up a parallel x86_64 Homebrew prefix under `/usr/local/` per the
  user's local `setup-x86_64-toolchain.sh` (Rosetta required on the runner)

This is the same recipe the local-build path uses today; it just isn't
needed in CI as long as native Intel runners exist.

**Tag race.** If two release tags are pushed within seconds of each other,
the workflow's "previous release" lookup for the auto-generated commit
list could produce an odd diff. Trivial to ignore in practice — date tags
don't collide except by deliberate same-day re-cuts (which use the `.N`
suffix and produce sensible diffs).
````

- [ ] **Step 2: Verify markdown renders cleanly (eyeball)**

Run: `head -30 /Users/mark/Documents/Projects/pcsx2-libretro/UPSTREAM-UPDATE.md`
Expected: well-formed markdown, no stray backticks or broken code fences.

- [ ] **Step 3: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add UPSTREAM-UPDATE.md
git commit -m "$(cat <<'EOF'
docs: add UPSTREAM-UPDATE.md — rebase + release process

Single source of truth for the monthly upstream rebase and release-cut
workflow. Covers the local steps, the tag→CI handoff, the discipline
of keeping the rebase surface tiny, and common CI/rebase failure modes.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Prepend fork-notice block to README.md

**Files:**
- Modify: `/Users/mark/Documents/Projects/pcsx2-libretro/README.md` (prepend block to top, leave all upstream PCSX2 content intact below)

The current `README.md` line 1 is `# PCSX2`. We prepend a small block above it.

- [ ] **Step 1: Read the current top of README.md to confirm the anchor**

Run: `head -2 /Users/mark/Documents/Projects/pcsx2-libretro/README.md`
Expected:
```
# PCSX2

```

- [ ] **Step 2: Prepend the fork-notice block**

Use the Edit tool to replace the existing `# PCSX2` line (and the blank line after it) with the fork-notice block followed by `# PCSX2` again.

`old_string`:
```
# PCSX2

```

`new_string`:
```
# pcsx2-libretro (RetroNest fork)

> This is a fork of [PCSX2](https://github.com/PCSX2/pcsx2) that builds a
> [libretro](https://www.libretro.com) core for use inside
> [RetroNest](https://github.com/markrpearce96/RetroNest). The upstream
> PCSX2 README follows below.
>
> - **Releases:** Pre-built macOS x86_64 cores at the [Releases tab](https://github.com/markrpearce96/pcsx2-libretro/releases).
> - **Building locally:** `cmake -S . -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF -DCMAKE_BUILD_TYPE=Release && cmake --build build --target pcsx2_libretro -j`
> - **Updating from upstream:** see [UPSTREAM-UPDATE.md](UPSTREAM-UPDATE.md).
> - **License:** GPL-3.0, inherited from PCSX2. See [bin/docs/License.txt](bin/docs/License.txt).
>
> All RetroNest-specific code lives in the `pcsx2-libretro/` directory.
> Upstream files are untouched except for a 4-line conditional in the
> top-level `CMakeLists.txt` and this fork-notice block.

---

# PCSX2

```

- [ ] **Step 3: Verify the prepend landed correctly**

Run: `head -20 /Users/mark/Documents/Projects/pcsx2-libretro/README.md`
Expected: starts with `# pcsx2-libretro (RetroNest fork)`, ends the visible window with the `# PCSX2` line (upstream content intact below).

Run: `grep -c "Codacy Badge" /Users/mark/Documents/Projects/pcsx2-libretro/README.md`
Expected: `1` (proves the upstream content survived the edit untouched).

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git add README.md
git commit -m "$(cat <<'EOF'
docs: add fork-notice block to README.md

Visitors landing on the GitHub repo see the fork context (what it is,
where releases live, how to build) before the upstream PCSX2 README.
Prepend-only modification — line-1 additions are the lowest-conflict
form of upstream-file edit and rebase trivially.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Manual GitHub setup — create remote repo

**Files:** None. This is a user-action checkpoint.

**This task requires the user to do something outside the terminal.** Stop and ask before proceeding past it.

- [ ] **Step 1: Ask the user to create the GitHub repo**

Tell the user:

> Please create an empty repository on GitHub:
>
> 1. Go to https://github.com/new
> 2. Owner: your account (e.g. `markrpearce96`)
> 3. Repository name: `pcsx2-libretro`
> 4. Description: "Libretro core fork of PCSX2 — built for RetroNest"
> 5. Public or Private — your choice (Public is recommended so RetroNest can fetch releases without auth)
> 6. **Do NOT** initialize with README, .gitignore, or LICENSE — we'll push everything from local.
> 7. Click "Create repository"
> 8. Copy the repo URL (the HTTPS one: `https://github.com/<you>/pcsx2-libretro.git`)
>
> Paste the URL back here so I can wire it up as `origin`.

Wait for the user's response. Capture the URL they provide as `$REPO_URL` for the next task.

- [ ] **Step 2: Verify the URL format**

Once the user provides a URL, sanity-check it:
- Starts with `https://github.com/`
- Ends with `.git`
- Path segments look reasonable (account/repo-name)

If the URL looks wrong, ask the user to confirm.

---

## Task 6: Push fork to origin

**Files:** None — git remote / push operations.

**Working directory:** `/Users/mark/Documents/Projects/pcsx2-libretro`

- [ ] **Step 1: Add origin remote**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git remote add origin "<URL from Task 5>"
git remote -v
```

Expected output now contains both:
```
origin	<URL>  (fetch)
origin	<URL>  (push)
upstream	https://github.com/PCSX2/pcsx2.git (fetch)
upstream	https://github.com/PCSX2/pcsx2.git (push)
```

- [ ] **Step 2: Push main (large — may take several minutes)**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git push -u origin main
```

Expected: ~1 GB upload. GitHub may print a warning about "large repository" (it's right at their soft limit) but should accept the push. If it hangs or fails, retry once.

Verify after success:
```bash
git -C /Users/mark/Documents/Projects/pcsx2-libretro rev-parse --abbrev-ref --symbolic-full-name @{u}
```
Expected: `origin/main`

- [ ] **Step 3: Confirm with user that the repo browses correctly**

Ask the user:
> Please visit the repo URL in your browser and confirm:
> - The repo page loads
> - The README shows the fork-notice block at the top
> - The Actions tab shows `libretro_release` listed (no runs yet — that's expected)

Wait for confirmation before proceeding to the smoke-cut tag.

---

## Task 7: Smoke-cut first release tag and watch CI

**Files:** None.

**Working directory:** `/Users/mark/Documents/Projects/pcsx2-libretro`

- [ ] **Step 1: Compute today's tag**

```bash
TAG="v$(date -u +%Y.%m.%d)"
echo "Tag will be: $TAG"
```

Expected: a single line like `Tag will be: v2026.05.21`.

- [ ] **Step 2: Create and push the tag**

```bash
cd /Users/mark/Documents/Projects/pcsx2-libretro
git tag "$TAG"
git push origin "$TAG"
```

Expected: successful push, output ending with `* [new tag]         v2026.05.21 -> v2026.05.21` (or similar).

- [ ] **Step 3: Watch CI**

Ask the user to open `https://github.com/<you>/pcsx2-libretro/actions` and report:
- Did the `libretro_release` workflow start?
- Is the `build` job running on `macos-13`?

If the workflow doesn't appear within 30 seconds of the tag push, something is wrong with the trigger configuration. Common causes:
- Tag pattern mismatch (we use `v*` — `v2026.05.21` matches)
- `.github/workflows/` directory not recognized (verify it's `lowercase .github` not `.Github`)
- Repo is empty of code (shouldn't be — we pushed `main` already)

- [ ] **Step 4: Iterate on build failures**

If `build` job fails, read the failed step's logs. Typical first-run failures:

| Error pattern | Fix |
|---|---|
| `brew: package not found` | Add the package to the `Install Homebrew dependencies` step in `libretro_release.yml`. Commit. Delete the failed tag locally + remotely, re-cut the same tag (`v2026.05.21.1`) to avoid noisy release noise. |
| `cmake: missing dependency` | Same as above — usually a brew package the configure step expects. |
| `target pcsx2_libretro not defined` | The CMake target is gated on something we didn't set. Check the configure step's CMake output for ENABLE_LIBRETRO. Verify `-DENABLE_LIBRETRO=ON` is on the configure command. |
| `lipo` / arch-related error | We're x86_64-only; if you see arm64 errors, check `CMAKE_OSX_ARCHITECTURES=x86_64` is being honored. |
| `softprops/action-gh-release: 403` | Workflow lacks `contents: write` permission. Confirmed in the YAML; if the error still appears, check repo Settings → Actions → Workflow permissions = "Read and write". |

To delete a failed tag and re-cut:
```bash
git tag -d v2026.05.21
git push origin :refs/tags/v2026.05.21
# (fix the YAML, commit, push main)
git tag v2026.05.21.1
git push origin v2026.05.21.1
```

Keep iterating until both jobs succeed. Expect 1–3 iterations on first-ever run; subsequent ships should be one-shot.

- [ ] **Step 5: Confirm both jobs green**

Ask the user to verify in the Actions UI that both `build` and `release` jobs show green checkmarks.

---

## Task 8: Verify the published release

**Files:** None. Smoke-test the published artifact.

- [ ] **Step 1: Find the release URL**

```bash
echo "https://github.com/<you>/pcsx2-libretro/releases/latest"
```

Ask the user to open this in a browser, OR run via `gh`:
```bash
gh release view --repo <you>/pcsx2-libretro
```

- [ ] **Step 2: Download the zip and inspect contents**

```bash
cd /tmp
gh release download --repo <you>/pcsx2-libretro --pattern 'pcsx2_libretro-macos-x86_64.zip'
unzip -l pcsx2_libretro-macos-x86_64.zip
```

Expected output contains three top-level entries:
- `pcsx2_libretro.dylib`
- `pcsx2_libretro_resources/` (with `GameIndex.yaml`, `default.metallib`, `fonts/`, etc. nested inside)
- `VERSION`

- [ ] **Step 3: Inspect the VERSION file**

```bash
cd /tmp
mkdir -p verify && cd verify
unzip -o ../pcsx2_libretro-macos-x86_64.zip
cat VERSION
```

Expected content (values will differ):
```
tag=v2026.05.21
platform=macos-x86_64
fork_sha=0751594fa1b2c3d4...
upstream_sha=...
upstream_ref=upstream/master
build_date=2026-05-21T14:32:18Z
```

All six keys present, all non-empty.

- [ ] **Step 4: Verify the dylib is x86_64 only**

```bash
cd /tmp/verify
file pcsx2_libretro.dylib
```

Expected: `Mach-O 64-bit dynamically linked shared library x86_64`

If it shows `arm64` or `universal binary`, the workflow's `CMAKE_OSX_ARCHITECTURES=x86_64` flag didn't take — investigate the configure step's output.

- [ ] **Step 5: Drop-in test (optional but recommended)**

Replace the local RetroNest core with the just-built one and verify it still launches:

```bash
cd /tmp/verify
cp pcsx2_libretro.dylib ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
rsync -a --delete pcsx2_libretro_resources/ ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/
```

Ask the user to launch RetroNest and boot a known-working game (e.g. Ratchet & Clank 2). If it works, the published artifact is functionally equivalent to a local build — the publishing pipeline is verified end-to-end.

- [ ] **Step 6: Clean up the /tmp verify dir**

```bash
rm -rf /tmp/verify /tmp/pcsx2_libretro-macos-x86_64.zip
```

---

## Task 9: Update auto-memory with publishing infrastructure

**Files:**
- Create: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_publishing_shipped.md`
- Modify: `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/MEMORY.md` (add one index line at top)

- [ ] **Step 1: Write the memory entry**

Replace `<REPO_URL>`, `<TAG>`, and `<FORK_SHA>` with the actual values from Tasks 5–8.

Write to `/Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_publishing_shipped.md`:

```markdown
---
name: session-handoff-publishing-shipped
description: pcsx2-libretro now published to GitHub at <REPO_URL>. Tag-triggered GHA workflow builds macOS x86_64 .dylib + resources zip on each `v*` tag and publishes a GitHub Release. RetroNest-side installer is a separate future sub-project.
metadata:
  type: project
---

**Shipped 2026-05-21.** Three commits on pcsx2-libretro `main` add the publishing infrastructure:

1. `ci: add libretro_release workflow` — `.github/workflows/libretro_release.yml`. Trigger: `push: tags: ['v*']`. Two jobs: `build` on `macos-13` (Homebrew deps, cmake configure with `-DENABLE_LIBRETRO=ON -DCMAKE_OSX_ARCHITECTURES=x86_64`, build `pcsx2_libretro` target, upload dylib + `bin/resources/` as artifacts), then `release` on `ubuntu-latest` (downloads both, generates `VERSION` file, zips into `pcsx2_libretro-macos-x86_64.zip`, publishes via `softprops/action-gh-release@v2` with auto-generated commit list).
2. `docs: add UPSTREAM-UPDATE.md` — rebase + release process. Codifies the 7-step monthly flow (fetch upstream → rebase → sanity build → optional local smoke → push origin → tag → CI ships it). Includes known failure modes for rebase (CMakeLists anchor, API drift, new brew dep) and CI (runner retirement, tag race).
3. `docs: fork-notice block in README.md` — small prepended block making the GitHub landing page identify the fork. Upstream PCSX2 README content survives intact below.

**Repo:** `<REPO_URL>`
**First release tag:** `<TAG>` (fork SHA `<FORK_SHA>`)

**Why:** RetroNest needs a public download source for the .dylib. The existing `PatchesInstaller` pattern (which fetches from PCSX2/pcsx2_patches releases) establishes the download-from-GitHub-Releases precedent. This ship gives RetroNest's future installer something to point at.

**How to apply:** When the user wants to ship a new pcsx2-libretro build, follow `UPSTREAM-UPDATE.md` in the fork. The whole cycle is: `git fetch upstream && git rebase upstream/master && git push origin main --force-with-lease && git tag v$(date +%Y.%m.%d) && git push origin <tag>`. CI does the rest.

## Discipline preserved

- Only two upstream-file modifications across the entire ship: the existing 4-line `CMakeLists.txt` block and the new fork-notice block at the top of `README.md`. Both are line-1 / line-N prepends, the lowest-conflict form of edit.
- No source code in `pcsx2-libretro/` was touched — this ship is pure infrastructure.

## Architecture decisions (recorded for future sub-projects)

- **x86_64 only on macOS.** Apple Silicon lacks upstream AArch64 recompilers; Rosetta x86_64 is 3.1× faster (SP10). Zip naming `pcsx2_libretro-macos-x86_64.zip` leaves clean room for `-windows-x86_64` (future Windows port) and `-macos-arm64` (if/when upstream lands recompilers).
- **Date-based tags `vYYYY.MM.DD`** (with `.N` suffix for same-day re-cuts). Continuous-deployment-style fork — semver would be invented policy.
- **Single zip with `VERSION` file** for staleness checking. Plain `key=value` so RetroNest's future installer needs no YAML/JSON dep.
- **`macos-13` runner today.** When GitHub retires it, switch to `macos-14` with Rosetta + x86_64 brew prefix. Path documented in `UPSTREAM-UPDATE.md`.

## Out-of-scope follow-ups (linked from [[project-pcsx2-libretro-port]])

- **RetroNest `Pcsx2CoreInstaller`** — mirrors `PatchesInstaller`. Fetches the zip from latest release at app start, staleness-gated by `VERSION` file's `tag` field, extracts into the cores directory. Separate sub-project.
- **Windows port (SP11?)** — needs HWND + D3D11/Vulkan render bridge replacing the current Metal/NSView path. Multi-week sub-project, not a CI tweak.
- **Native macOS arm64** — viable when upstream PCSX2 lands AArch64 recompilers.
- **Code signing / notarization** — libretro cores aren't gatekeeper-checked when loaded by a notarized host. Revisit only if RetroNest's gatekeeper posture changes.

Spec: `RetroNest-Project/docs/superpowers/specs/2026-05-21-pcsx2-libretro-github-publishing-design.md`
Plan: `RetroNest-Project/docs/superpowers/plans/2026-05-21-pcsx2-libretro-github-publishing.md`
```

- [ ] **Step 2: Add the index line to MEMORY.md**

Use the Edit tool to add a new line at the top of the `MEMORY.md` index (immediately after the first `- [...` line that exists today).

Read `MEMORY.md` first to confirm the current top entry, then prepend:

```
- [**Publishing infrastructure SHIPPED 2026-05-21**](session_handoff_publishing_shipped.md) — pcsx2-libretro now lives at <REPO_URL>. Tag-triggered GHA workflow ships `pcsx2_libretro-macos-x86_64.zip` to GitHub Releases on each `v*` tag. RetroNest-side installer deferred to a future sub-project.
```

(Replace `<REPO_URL>` with the actual URL.)

- [ ] **Step 3: Verify memory updates**

Run: `head -3 /Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/MEMORY.md`
Expected: first content line is the new publishing index entry.

Run: `ls -la /Users/mark/.claude/projects/-Users-mark-Documents-Projects-pcsx2-libretro/memory/session_handoff_publishing_shipped.md`
Expected: file exists, non-zero size.

---

## Plan Complete

After Task 9, the publishing infrastructure is shipped, verified end-to-end, and the auto-memory is updated for future sessions to discover.

**Acceptance criteria recap:**
- [ ] Empty GitHub repo exists at the user's chosen URL
- [ ] `origin` remote configured locally; `main` pushed
- [ ] `.github/workflows/libretro_release.yml`, `UPSTREAM-UPDATE.md`, README.md fork-notice block all committed and pushed
- [ ] First tag `v2026.05.21` triggered the workflow; both `build` and `release` jobs green
- [ ] Published release contains `pcsx2_libretro-macos-x86_64.zip` with dylib + resources + VERSION
- [ ] The published dylib is x86_64-only (`file` reports `x86_64`, not universal)
- [ ] (Optional) drop-in replacement of the local RetroNest core boots a known-good game
- [ ] Auto-memory updated with publishing-shipped entry
