#!/usr/bin/env bash
# Build a universal RetroNest.app + every shipped libretro core, then
# install cores to ~/Documents/RetroNest/emulators/libretro/cores/.
#
# Prerequisites: ./scripts/setup-x86_64-toolchain.sh has been run.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Paths to source trees outside this repo.
PCSX2_SRC="/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master"
# mGBA: edit if your local clone lives elsewhere.
MGBA_SRC="${MGBA_SRC:-$HOME/Documents/Projects/mgba-libretro}"

CORES_DIR="$HOME/Documents/RetroNest/emulators/libretro/cores"

# 1. Preflight: both brew prefixes have qt.
echo "=== preflight ==="
test -f /opt/homebrew/opt/qt/lib/QtCore.framework/Versions/A/QtCore \
    || { echo "arm64 Qt missing — run scripts/setup-x86_64-toolchain.sh"; exit 1; }
test -f /usr/local/opt/qt/lib/QtCore.framework/Versions/A/QtCore \
    || { echo "x86_64 Qt missing — run scripts/setup-x86_64-toolchain.sh"; exit 1; }

# Optional: warn on version drift between prefixes.
arm_qt="$(/opt/homebrew/bin/brew --prefix qt@6 2>/dev/null | xargs -I{} basename {})"
x86_qt="$(arch -x86_64 /usr/local/bin/brew --prefix qt@6 2>/dev/null | xargs -I{} basename {})"
[[ "$arm_qt" == "$x86_qt" ]] || echo "  ! Qt prefix drift: arm64=$arm_qt x86_64=$x86_qt (continuing)"

# 2. Build RetroNest.app (arm64 + x86_64).
echo "=== building RetroNest arm64 ==="
arch -arm64 cmake -S cpp -B cpp/build-arm64 \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/sdl2"
arch -arm64 cmake --build cpp/build-arm64 -j
/opt/homebrew/opt/qt/bin/macdeployqt cpp/build-arm64/RetroNest.app -qmldir=qml

echo "=== building RetroNest x86_64 ==="
arch -x86_64 cmake -S cpp -B cpp/build-x86_64 \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local/opt/qt;/usr/local/opt/sdl2"
arch -x86_64 cmake --build cpp/build-x86_64 -j
arch -x86_64 /usr/local/opt/qt/bin/macdeployqt cpp/build-x86_64/RetroNest.app -qmldir=qml

echo "=== merging RetroNest.app ==="
./scripts/lipo-merge-app.sh \
    cpp/build-arm64/RetroNest.app \
    cpp/build-x86_64/RetroNest.app \
    cpp/build-universal/RetroNest.app \
    cpp/resources/RetroNest.entitlements

# 3. Build pcsx2_libretro.dylib (arm64 + x86_64).
echo "=== building pcsx2_libretro arm64 ==="
( cd "$PCSX2_SRC" && arch -arm64 cmake -B build-arm64 \
    -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_PREFIX_PATH="/opt/homebrew" \
  && arch -arm64 cmake --build build-arm64 --target pcsx2_libretro -j )

echo "=== building pcsx2_libretro x86_64 ==="
( cd "$PCSX2_SRC" && arch -x86_64 cmake -B build-x86_64 \
    -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_PREFIX_PATH="/usr/local" \
  && arch -x86_64 cmake --build build-x86_64 --target pcsx2_libretro -j )

./scripts/lipo-merge-dylib.sh \
    "$PCSX2_SRC/build-arm64/pcsx2-libretro/pcsx2_libretro.dylib" \
    "$PCSX2_SRC/build-x86_64/pcsx2-libretro/pcsx2_libretro.dylib" \
    "$CORES_DIR/pcsx2_libretro.dylib"

# 4. Build mgba_libretro.dylib (arm64 + x86_64) if source tree available.
if [[ -d "$MGBA_SRC" ]]; then
    echo "=== building mgba_libretro arm64 ==="
    ( cd "$MGBA_SRC" && arch -arm64 cmake -B build-arm64 \
        -DBUILD_LIBRETRO=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
      && arch -arm64 cmake --build build-arm64 --target mgba_libretro -j )

    echo "=== building mgba_libretro x86_64 ==="
    ( cd "$MGBA_SRC" && arch -x86_64 cmake -B build-x86_64 \
        -DBUILD_LIBRETRO=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF -DBUILD_GL=OFF \
        -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_PREFIX_PATH="/usr/local" \
      && arch -x86_64 cmake --build build-x86_64 --target mgba_libretro -j )

    ./scripts/lipo-merge-dylib.sh \
        "$MGBA_SRC/build-arm64/mgba_libretro.dylib" \
        "$MGBA_SRC/build-x86_64/mgba_libretro.dylib" \
        "$CORES_DIR/mgba_libretro.dylib"
else
    echo "  ! mGBA source tree not found at $MGBA_SRC — skipping; "
    echo "    set MGBA_SRC=<path> to enable."
fi

# 5. Run the verification gate.
./scripts/verify-universal.sh

echo
echo "✓ Universal build complete."
echo "  RetroNest.app: cpp/build-universal/RetroNest.app"
echo "  Cores:         $CORES_DIR/"
