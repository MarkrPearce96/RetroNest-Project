#pragma once
#include "../pcsx2/pcsx2_theme.h"

// PPSSPP settings dialog uses the same warm-grey + amber palette as
// PCSX2 and DuckStation. Spec 2026-05-02 explicitly defers any per-
// emulator visual divergence — this header is a thin alias so that
// PPSSPP code can reference PpssppTheme:: without coupling directly
// to Pcsx2Theme.
namespace PpssppTheme {
    using namespace Pcsx2Theme;
}
