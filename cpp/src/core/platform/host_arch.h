#pragma once

// Compile-time host-arch flag. Universal builds compile this header twice
// (once per slice); each slice's binary returns the matching value at
// runtime. On Apple Silicon hardware (the only target), the x86_64 slice
// running means the process is under Rosetta — no sysctl.proc_translated
// runtime check needed.
namespace HostArch {
    constexpr bool isArm64() {
    #if defined(__aarch64__)
        return true;
    #else
        return false;
    #endif
    }

    constexpr bool isRosettaX86_64() { return !isArm64(); }
}
