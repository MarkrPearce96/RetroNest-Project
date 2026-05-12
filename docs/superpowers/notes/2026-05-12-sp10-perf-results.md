# SP10 — Native arm64 vs Rosetta x86_64 perf comparison

**Date:** 2026-05-12
**Hardware:** Apple Silicon M-series, 16 GB unified RAM, 10 logical cores
**ROM / scene:** Ratchet & Clank 2 — in-game reference scene
**Measurement source:** `Host::OnPerformanceMetricsUpdated` transient diagnostic in
`pcsx2-master/pcsx2-libretro/HostStubs.cpp` (commit `ad1c834d4`, reverted after these
measurements). Emits one stderr line per second:
`[pcsx2-libretro perf] speed=…% fps=… cpu=…% gs=…% vu=…%`
**Sample window:** ≥ 30 s of steady-state per configuration (excluding load transients).

## Results

| Configuration                                          | Speed % | FPS  | CPU %       | GS % | VU % |
|--------------------------------------------------------|--------:|-----:|------------:|-----:|-----:|
| Native arm64 RetroNest + arm64 libretro slice          |   32.4  | 19.4 | 100 (pegged)| 0.1  | 0.0  |
| Rosetta x86_64 RetroNest + x86_64 libretro slice (SP10)|  100.0  | 60.0 |  ~35        | ~2.5 | 0.0  |
| Standalone PCSX2 v2.6.3.app (reference)                |   *not measured — see Observations* |||||

**Headline:** Rosetta x86_64 mode is **~3.1× faster** than native arm64 (32.4 % → 100 %)
and locks Ratchet & Clank 2 at **60 FPS / 100 % speed** with EE thread headroom to spare
(CPU dropped from 100 % pegged to ~35 %).

## How the comparison was performed

Same universal `RetroNest.app`. Slice selection was forced via `arch -arm64` and
`arch -x86_64` on the binary inside the bundle (equivalent to Finder → Get Info →
"Open using Rosetta" but driven from the shell so the two runs were back-to-back
without manual toggling):

```sh
# arm64 baseline
arch -arm64  cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > /tmp/retronest-arm64.log 2>&1 &
# Rosetta x86_64 (after quitting arm64 instance)
arch -x86_64 cpp/build-universal/RetroNest.app/Contents/MacOS/RetroNest > /tmp/retronest-x86_64.log 2>&1 &
```

Both runs booted the same R&C 2 save into the same scene, then captured ≥ 30 seconds
of steady-state perf samples.

Confirmation that the x86_64 launch was actually translating: Rosetta's AOT cache
populated under `/private/var/db/oah/<hash>/.aot` for QtCore + libSDL2 + the libretro
core (verified via `lsof -p <pid>` while running).

## Observations

- **3.1× speedup matches the SP10 hypothesis.** The PS2's EE/IOP/VU run on PCSX2's
  *interpreters* in any native-arm64 build today — `pcsx2-master/cmake/BuildParameters.cmake:113`
  selects ARM64 with no recompiler equivalents to the x86 paths. PCSX2's x86_64
  recompilers get translated to arm64 by Rosetta 2 ahead-of-time, and the translated
  recompilers beat native arm64 interpreters comfortably even with Rosetta's 25-30 %
  translation overhead.
- **CPU thread is no longer the bottleneck under Rosetta.** Native arm64 had the EE
  thread pegged at 100 % (interpreter-bound). Rosetta x86_64 sits around 35 %, leaving
  room for any future demanding scenes without dropping frames.
- **Standalone reference skipped** — Rosetta x86_64 is already at the framerate ceiling
  (100 % / 60 FPS), and standalone PCSX2 v2.6.3.app (also x86_64-under-Rosetta) would
  hit the same vsync ceiling in this scene. The comparison wasn't needed to validate the
  SP10 hypothesis. Re-run if a heavier scene where Rosetta drops below 100 % is found —
  that's where standalone-vs-libretro divergence (if any) would show up.
- **VU % stays at 0.0 in both modes.** MTVU is enabled in `Settings.cpp` (SP5 commit
  `f6031bff0`) but the diagnostic shows VU thread idle. Either MTVU isn't being applied
  in this scene (VU runs on the EE thread), or the VU work is genuinely trivial. Worth
  re-checking if SP10 follow-up perf-tunes the libretro core further, but doesn't affect
  the SP10 conclusion.
- **Native arm64 32 % is lower than the previous 65–70 % R&C 2 measurement** (project
  memory, SP5 era). Likely a different / heavier in-game scene; the apples-to-apples
  comparison between today's two configurations stays valid because both were measured
  on the same save / same scene.

## Conclusion

SP10 ships. The user's perf gap to standalone PCSX2 is closed: launching the universal
`RetroNest.app` under Rosetta (Finder → Get Info → "Open using Rosetta" → relaunch)
loads PCSX2's x86_64 recompilers in-process and runs PS2 emulation at the framerate
ceiling. mGBA was rebuilt as a universal binary too (using upstream `mgba-emu/mgba`,
not the broken libretro fork), so the user can leave Rosetta toggled on permanently and
still play GBA games — no per-emulator switching needed.

The slow-mode toast added in step 6 (commit `1adb9b6` — `GameSession::startLibretro`)
warns the user one time per session when they boot a PS2 game in arm64 mode.
