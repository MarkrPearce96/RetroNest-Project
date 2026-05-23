# MoltenVK ↔ Metal Interop Spike

SP0 of the Dolphin libretro conversion. Answers: can we share a texture between
MoltenVK and Metal without an extra copy?

See: `docs/superpowers/notes/2026-05-23-moltenvk-metal-interop-spike.md` for findings.

## Build

```
cmake -B build \
    -DMOLTENVK_DYLIB=/usr/local/lib/libMoltenVK.dylib \
    -DVULKAN_HEADERS=/usr/local/include \
    -DVULKAN_LOADER_DYLIB=/usr/local/lib/libvulkan.dylib
cmake --build build
./build/moltenvk_metal_interop_spike
```

Output indicates whether Path A and/or Path B work. Returns exit 0 if at least
one path succeeds.

## Cleanup

Delete this directory once SP4 ships and the LibretroVulkanItem implementation
has settled.
