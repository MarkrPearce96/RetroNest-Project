#!/usr/bin/env bash
# Idempotent one-time setup for the x86_64 build toolchain.
# Installs a parallel x86_64 Homebrew at /usr/local plus every dep the
# universal RetroNest build needs in x86_64 form. Safe to re-run.
set -euo pipefail

if [[ "$(uname -m)" != "arm64" ]]; then
    echo "This script targets Apple Silicon hosts. Detected $(uname -m)." >&2
    exit 1
fi

if ! arch -x86_64 /usr/bin/true 2>/dev/null; then
    echo "Rosetta 2 is not installed. Run: softwareupdate --install-rosetta" >&2
    exit 1
fi

# 1. Install x86_64 Homebrew at /usr/local if missing.
if [[ ! -x /usr/local/bin/brew ]]; then
    echo "Installing x86_64 Homebrew at /usr/local..."
    arch -x86_64 /bin/bash -c \
        "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
else
    echo "x86_64 Homebrew already at /usr/local — updating."
    arch -x86_64 /usr/local/bin/brew update
fi

# 2. Install deps idempotently (RetroNest + PCSX2 + mGBA).
# PCSX2 dep list sourced from pcsx2-master/.github/workflows/macos-build.yml.
DEPS=(
    qt@6 sdl2
    fmt webp libzip libpng zstd lz4
    sound-touch ffmpeg
    pkgconf cmake ninja
)

for dep in "${DEPS[@]}"; do
    if arch -x86_64 /usr/local/bin/brew list --formula "$dep" >/dev/null 2>&1; then
        echo "  ✓ $dep"
    else
        echo "  → installing $dep (x86_64)"
        arch -x86_64 /usr/local/bin/brew install "$dep"
    fi
done

echo
echo "Done. /usr/local/bin/brew is the x86_64 brew; /opt/homebrew/bin/brew is the arm64 brew."
echo "Run \`scripts/build-universal.sh\` to build."
