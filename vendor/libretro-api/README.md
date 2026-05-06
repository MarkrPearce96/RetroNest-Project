# libretro API header (vendored)

Source: https://github.com/libretro/RetroArch — `libretro-common/include/libretro.h`
License: MIT (see header comment in `libretro.h`)

This is a single-file C header defining the libretro ABI. We vendor it instead
of pulling the full libretro-common because we only need the public ABI
declarations, not the helper utilities.

To refresh:
    curl -fsSL -o libretro.h \
      https://raw.githubusercontent.com/libretro/RetroArch/master/libretro-common/include/libretro.h
