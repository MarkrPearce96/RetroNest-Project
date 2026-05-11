# PCSX2 Libretro Audio Output (SP4) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route PCSX2's SPU2 audio output through libretro's `retro_audio_sample_batch_t` callback so a real PS2 game (Ratchet & Clank) plays audible, in-sync stereo audio inside RetroNest.

**Architecture:** A new `LibretroAudioStream` subclass of PCSX2's `AudioStream` is selected via a new `AudioBackend::Libretro` enum entry. SPU2 fills the inherited lock-free ring buffer (with full SoundTouch timestretch). `retro_run`, after waiting on the existing SP3 `g_present_cv` video signal, drains accumulated frames via a public `DrainToLibretroCallback` method, converts float→int16 stereo, and calls the cached `audio_sample_batch_cb`. Total upstream-file deviation: 5 lines across 2 files in `pcsx2/Host/`.

**Tech Stack:** C++20, CMake, PCSX2 2.x master fork (`retronest-libretro` branch), libretro C ABI. Built into `pcsx2_libretro.dylib`. No new dependencies.

**Spec:** [`docs/superpowers/specs/2026-05-11-pcsx2-libretro-audio-output-design.md`](../specs/2026-05-11-pcsx2-libretro-audio-output-design.md)

**Testing model:** This is a libretro shim with no existing unit-test infrastructure (matches SP1–SP3.5 plans). Verification is **build-driven** (each task ends with a successful `cmake --build`) plus a final **end-to-end smoke test** on Ratchet & Clank. Don't try to TDD this — there are no test fixtures and creating them is out of scope for SP4.

**Working directory:** `/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/` (note trailing space in folder name — always quote). Branch: `retronest-libretro`. Build dir: `build/`.

**Build command (used at the end of every task):**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && cmake --build build --target pcsx2_libretro -j8
```

Expected: build succeeds, `build/pcsx2-libretro/pcsx2_libretro.dylib` updates. After the final task, copy the dylib into RetroNest:

```sh
cp "build/pcsx2-libretro/pcsx2_libretro.dylib" ~/Documents/RetroNest/emulators/libretro/cores/
```

---

## File Structure

**Modified upstream files (5 lines total, comment-flagged for rebase reviewers):**
- `pcsx2/Host/AudioStreamTypes.h` — +1 line: `Libretro` enum entry
- `pcsx2/Host/AudioStream.cpp` — +4 lines: name array entries, dispatch case, extern decl

**Modified pcsx2-libretro shim files:**
- `pcsx2-libretro/CMakeLists.txt` — +1 line: add `LibretroAudioStream.cpp` to `target_sources`
- `pcsx2-libretro/Settings.cpp` — replace `OutputModule = "nullout"` with `Backend = Libretro` + force stereo
- `pcsx2-libretro/LibretroFrontend.cpp` — +~6 lines: drain audio in `retro_run` after present_cv

**New pcsx2-libretro shim files:**
- `pcsx2-libretro/LibretroAudioStream.h` — class declaration + singleton accessor
- `pcsx2-libretro/LibretroAudioStream.cpp` — implementation (mirrors `SDLAudioStream.cpp` shape)

**No changes to:** `HostStubs.cpp`, `EmuThread.cpp`, `LibretroFrontend.h` (audio_batch_cb already a field in `FrontendState`), `Settings.h`.

---

## Task 1 — Add `AudioBackend::Libretro` to PCSX2's enum and factory

Adds the new backend identity. After this task, the enum compiles and an empty default switch case routes Libretro → fallback path. `LibretroAudioStream` doesn't exist yet — that's Task 2/3. So we add a forward `extern` declaration whose definition will land in Task 3.

**Files:**
- Modify: `pcsx2-master/pcsx2/Host/AudioStreamTypes.h:10-16`
- Modify: `pcsx2-master/pcsx2/Host/AudioStream.cpp:148-157`, `:107-129`

- [ ] **Step 1.1: Add `Libretro` to the AudioBackend enum**

In `pcsx2-master/pcsx2/Host/AudioStreamTypes.h`, change:

```cpp
enum class AudioBackend : u8
{
	Null,
	Cubeb,
	SDL,
	Count
};
```

to:

```cpp
enum class AudioBackend : u8
{
	Null,
	Cubeb,
	SDL,
	Libretro, // pcsx2-libretro: routes samples to retro_audio_sample_batch_t (SP4)
	Count
};
```

The comment makes the rebase deviation visible.

- [ ] **Step 1.2: Add Libretro entries to the name arrays**

In `pcsx2-master/pcsx2/Host/AudioStream.cpp`, change:

```cpp
static constexpr const std::array s_backend_names = {
	"Null",
	"Cubeb",
	"SDL",
};
static constexpr const std::array s_backend_display_names = {
	TRANSLATE_NOOP("AudioStream", "Null (No Output)"),
	TRANSLATE_NOOP("AudioStream", "Cubeb"),
	TRANSLATE_NOOP("AudioStream", "SDL"),
};
```

to:

```cpp
static constexpr const std::array s_backend_names = {
	"Null",
	"Cubeb",
	"SDL",
	"Libretro", // pcsx2-libretro (SP4)
};
static constexpr const std::array s_backend_display_names = {
	TRANSLATE_NOOP("AudioStream", "Null (No Output)"),
	TRANSLATE_NOOP("AudioStream", "Cubeb"),
	TRANSLATE_NOOP("AudioStream", "SDL"),
	TRANSLATE_NOOP("AudioStream", "Libretro"), // pcsx2-libretro (SP4)
};
```

`ParseBackendName` / `GetBackendName` / `GetBackendDisplayName` index these arrays — no further edits to those functions.

- [ ] **Step 1.3: Add the dispatch case + extern declaration in CreateStream**

In `pcsx2-master/pcsx2/Host/AudioStream.cpp`, find `AudioStream::CreateStream` (around line 107). Change the body of the switch:

```cpp
	switch (backend)
	{
		case AudioBackend::Cubeb:
			return CreateCubebAudioStream(sample_rate, parameters, driver_name, device_name, stretch_enabled, error);

		case AudioBackend::SDL:
			return CreateSDLAudioStream(sample_rate, parameters, stretch_enabled, error);

		case AudioBackend::Null:
			return CreateNullStream(sample_rate, parameters.buffer_ms);

		default:
			Error::SetStringView(error, "Unknown audio backend.");
			return nullptr;
	}
```

to:

```cpp
	switch (backend)
	{
		case AudioBackend::Cubeb:
			return CreateCubebAudioStream(sample_rate, parameters, driver_name, device_name, stretch_enabled, error);

		case AudioBackend::SDL:
			return CreateSDLAudioStream(sample_rate, parameters, stretch_enabled, error);

		case AudioBackend::Libretro: // pcsx2-libretro (SP4) — defined in pcsx2-libretro/LibretroAudioStream.cpp
			return CreateLibretroAudioStream(sample_rate, parameters, stretch_enabled, error);

		case AudioBackend::Null:
			return CreateNullStream(sample_rate, parameters.buffer_ms);

		default:
			Error::SetStringView(error, "Unknown audio backend.");
			return nullptr;
	}
```

Then, **above** `AudioStream::CreateStream` (right after the closing brace of the previous function, which is `GetMSForBufferSize` near line 146) add:

```cpp
// Forward declaration — defined in pcsx2-libretro/LibretroAudioStream.cpp.
// Resolves only when the libretro core is built (ENABLE_LIBRETRO=ON);
// upstream PCSX2 builds never reference it because Backend defaults to Cubeb.
extern std::unique_ptr<AudioStream> CreateLibretroAudioStream(u32 sample_rate,
	const AudioStreamParameters& parameters, bool stretch_enabled, Error* error);
```

- [ ] **Step 1.4: Verify the build still compiles (link will fail — that's expected)**

Run:

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && cmake --build build --target pcsx2_libretro -j8
```

Expected: **link failure** with `Undefined symbol: CreateLibretroAudioStream(...)`. The compile of `AudioStream.cpp` should succeed. If you get a *compile* error in `AudioStream.cpp` or `AudioStreamTypes.h`, that's a real bug — re-read steps 1.1–1.3.

- [ ] **Step 1.5: Commit**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && git add pcsx2/Host/AudioStreamTypes.h pcsx2/Host/AudioStream.cpp && git commit -m "SP4 step 1: add AudioBackend::Libretro enum + factory dispatch

Five lines across two upstream files. Comment-flagged for rebase
reviewers. Definition of CreateLibretroAudioStream lands in next commit."
```

---

## Task 2 — `LibretroAudioStream.h`: class declaration + singleton accessor

Declares the subclass and the `s_active_stream` static pointer that lets `retro_run` find the live stream from outside `pcsx2-libretro/Settings.cpp`.

**Files:**
- Create: `pcsx2-master/pcsx2-libretro/LibretroAudioStream.h`

- [ ] **Step 2.1: Create the header**

Write `pcsx2-master/pcsx2-libretro/LibretroAudioStream.h`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// LibretroAudioStream — PCSX2 AudioStream subclass that routes SPU2 output
// to libretro's retro_audio_sample_batch_t callback.
//
// SPU2 produces 64-frame stereo float chunks via WriteChunk on the EE/SPU2
// thread (inherited base behavior — runs SoundTouch stretch + writes to the
// lock-free ring buffer). LibretroFrontend's retro_run drains the ring on
// the main thread by calling DrainToLibretroCallback, which converts float
// to int16 stereo and pushes via the supplied callback.
//
// Lifetime: instances are owned by SPU2 (s_output_stream). Constructor sets
// the s_active_stream singleton; destructor clears it. retro_run checks
// ActiveStream() before draining so it cleanly skips when no VM is running.

#pragma once

#include "pcsx2/Host/AudioStream.h"
#include "libretro.h"

namespace Pcsx2Libretro
{

class LibretroAudioStream final : public ::AudioStream
{
public:
    LibretroAudioStream(u32 sample_rate, const AudioStreamParameters& parameters);
    ~LibretroAudioStream() override;

    // Drains up to max_frames stereo frames from the ring buffer, converts
    // float to int16, and calls cb with the result. Safe to call when the
    // ring is empty (no-op). Caller must guarantee cb is non-null.
    //
    // Returns the number of stereo frames the libretro frontend accepted.
    // Any unaccepted frames are re-queued for the next call via an internal
    // staging buffer.
    u32 DrainToLibretroCallback(retro_audio_sample_batch_t cb, u32 max_frames);

    // Returns the live LibretroAudioStream if SPU2 currently owns one,
    // or nullptr otherwise. retro_run uses this to decide whether to drain.
    static LibretroAudioStream* ActiveStream();

private:
    // Used by DrainToLibretroCallback to retry frames the frontend didn't
    // consume on the previous call.
    static constexpr u32 MAX_FRAMES_PER_DRAIN = 2048;

    u32 m_pending_frames = 0;
    int16_t m_pending_buffer[MAX_FRAMES_PER_DRAIN * 2] = {};

    // One-shot diagnostic latch.
    bool m_first_drain_logged = false;
};

} // namespace Pcsx2Libretro

// Free function declared in pcsx2/Host/AudioStream.cpp via extern; defined
// in LibretroAudioStream.cpp. Lives outside the namespace because that's
// the signature AudioStream.cpp expects.
std::unique_ptr<AudioStream> CreateLibretroAudioStream(u32 sample_rate,
    const AudioStreamParameters& parameters, bool stretch_enabled, Error* error);
```

- [ ] **Step 2.2: No build (header-only — Task 3 builds the implementation)**

Skip the build for this step; we'll build at the end of Task 3 when there's something to link.

- [ ] **Step 2.3: Commit**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && git add pcsx2-libretro/LibretroAudioStream.h && git commit -m "SP4 step 2: LibretroAudioStream.h — class declaration

Subclass interface + singleton accessor + factory free function decl.
Implementation in next commit."
```

---

## Task 3 — `LibretroAudioStream.cpp`: constructor, destructor, factory, singleton

The minimum body that lets the link succeed. `DrainToLibretroCallback` lands as a stub here and gets a real implementation in Task 4.

**Files:**
- Create: `pcsx2-master/pcsx2-libretro/LibretroAudioStream.cpp`
- Modify: `pcsx2-master/pcsx2-libretro/CMakeLists.txt:11-16`

- [ ] **Step 3.1: Create the implementation file**

Write `pcsx2-master/pcsx2-libretro/LibretroAudioStream.cpp`:

```cpp
// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// LibretroAudioStream — implementation. Mirrors SDLAudioStream.cpp's
// shape; the meaningful difference is the consumer is on the main
// (retro_run) thread instead of an audio-device callback thread.

#include "pcsx2/PrecompiledHeader.h"

#include "LibretroAudioStream.h"
#include "LibretroFrontend.h"

#include "common/Assertions.h"
#include "common/Error.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace Pcsx2Libretro
{
namespace
{
    // Singleton pointer set by ctor / cleared by dtor. Atomic because the
    // EE thread's WriteChunk path doesn't read it (it goes through SPU2's
    // own s_output_stream pointer), but retro_run on the main thread must
    // see a coherent value relative to ctor/dtor on the CPU thread.
    std::atomic<LibretroAudioStream*> s_active_stream{nullptr};
} // namespace

LibretroAudioStream::LibretroAudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
    : AudioStream(sample_rate, parameters)
{
    // expansion_mode is forced to Disabled by Settings.cpp — verify here.
    pxAssert(parameters.expansion_mode == AudioExpansionMode::Disabled);

    // Stereo path uses StereoSampleReaderImpl directly. stretch_enabled is
    // wired through by CreateLibretroAudioStream below.
    BaseInitialize(&AudioStream::StereoSampleReaderImpl, /*stretch_enabled=*/false);

    LibretroAudioStream* expected = nullptr;
    if (!s_active_stream.compare_exchange_strong(expected, this))
    {
        // Should never happen — SPU2 destroys the previous stream before
        // creating the new one (spu2.cpp:117). Log loudly if it does.
        FrontendLog(RETRO_LOG_ERROR,
            "LibretroAudioStream: s_active_stream already set on construction (was %p)",
            static_cast<void*>(expected));
    }

    FrontendLog(RETRO_LOG_INFO,
        "LibretroAudioStream constructed: sample_rate=%u channels=%u",
        sample_rate, GetInternalChannels());
}

LibretroAudioStream::~LibretroAudioStream()
{
    LibretroAudioStream* expected = this;
    s_active_stream.compare_exchange_strong(expected, nullptr);
    FrontendLog(RETRO_LOG_INFO, "LibretroAudioStream destroyed");
}

LibretroAudioStream* LibretroAudioStream::ActiveStream()
{
    return s_active_stream.load(std::memory_order_acquire);
}

u32 LibretroAudioStream::DrainToLibretroCallback(retro_audio_sample_batch_t /*cb*/, u32 /*max_frames*/)
{
    // Stub — implemented in Task 4. Returning 0 keeps the link valid and
    // produces silence (acceptable interim behavior).
    return 0;
}

} // namespace Pcsx2Libretro

// Out-of-namespace factory matching the extern declaration in AudioStream.cpp.
std::unique_ptr<AudioStream> CreateLibretroAudioStream(u32 sample_rate,
    const AudioStreamParameters& parameters, bool stretch_enabled, Error* /*error*/)
{
    auto stream = std::make_unique<Pcsx2Libretro::LibretroAudioStream>(sample_rate, parameters);

    // BaseInitialize was called with stretch_enabled=false in the ctor.
    // If SPU2 wants stretch, re-init now via the public SetStretchEnabled.
    if (stretch_enabled)
        stream->SetStretchEnabled(true);

    return stream;
}
```

- [ ] **Step 3.2: Wire the new file into the libretro CMakeLists**

In `pcsx2-master/pcsx2-libretro/CMakeLists.txt`, change:

```cmake
target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    Settings.cpp
    EmuThread.cpp
)
```

to:

```cmake
target_sources(pcsx2_libretro PRIVATE
    LibretroFrontend.cpp
    HostStubs.cpp
    Settings.cpp
    EmuThread.cpp
    LibretroAudioStream.cpp
)
```

- [ ] **Step 3.3: Build**

Run:

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && cmake --build build --target pcsx2_libretro -j8
```

Expected: **build succeeds**. The `Undefined symbol: CreateLibretroAudioStream` link error from Task 1 is gone. If you get a compile error, the most likely culprits are:
- Missing include (`<atomic>`, `<algorithm>`, etc.) — add it.
- `BaseInitialize` is `protected` — confirm `LibretroAudioStream` is publicly inheriting.

- [ ] **Step 3.4: Commit**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && git add pcsx2-libretro/LibretroAudioStream.cpp pcsx2-libretro/CMakeLists.txt && git commit -m "SP4 step 3: LibretroAudioStream skeleton + CMake wire-up

Stream constructs / destructs cleanly and registers/unregisters the
singleton. DrainToLibretroCallback returns 0 (silence) — real impl
in next commit. Build links."
```

---

## Task 4 — Implement `DrainToLibretroCallback`

The actual sample drain + float→int16 conversion + callback invocation, with re-queue handling for partial libretro consumption.

**Files:**
- Modify: `pcsx2-master/pcsx2-libretro/LibretroAudioStream.cpp` (replace the stub `DrainToLibretroCallback` body)

- [ ] **Step 4.1: Replace the stub with the real implementation**

In `pcsx2-master/pcsx2-libretro/LibretroAudioStream.cpp`, find:

```cpp
u32 LibretroAudioStream::DrainToLibretroCallback(retro_audio_sample_batch_t /*cb*/, u32 /*max_frames*/)
{
    // Stub — implemented in Task 4. Returning 0 keeps the link valid and
    // produces silence (acceptable interim behavior).
    return 0;
}
```

Replace with:

```cpp
u32 LibretroAudioStream::DrainToLibretroCallback(retro_audio_sample_batch_t cb, u32 max_frames)
{
    if (!cb)
        return 0;

    max_frames = std::min(max_frames, MAX_FRAMES_PER_DRAIN);

    // Step 1: flush any frames the frontend rejected on the previous call.
    // m_pending_frames is non-zero only if the previous cb() returned less
    // than we passed it. We push those first so order is preserved.
    if (m_pending_frames > 0)
    {
        const size_t accepted = cb(m_pending_buffer, m_pending_frames);
        if (accepted < m_pending_frames)
        {
            // Frontend still backed up. Shift unaccepted frames to the front.
            const u32 remaining = static_cast<u32>(m_pending_frames - accepted);
            std::memmove(m_pending_buffer, m_pending_buffer + accepted * 2,
                         remaining * 2 * sizeof(int16_t));
            m_pending_frames = remaining;
            return 0; // Don't try to drain new frames while backed up.
        }
        m_pending_frames = 0;
    }

    // Step 2: read up to max_frames new frames from the ring buffer into a
    // float staging buffer, then convert to int16 stereo.
    const u32 available = std::min(GetBufferedFramesRelaxed(), max_frames);
    if (available == 0)
        return 0;

    float float_staging[MAX_FRAMES_PER_DRAIN * 2];
    ReadFrames(float_staging, available);

    int16_t int_staging[MAX_FRAMES_PER_DRAIN * 2];
    const u32 sample_count = available * 2; // stereo
    for (u32 i = 0; i < sample_count; ++i)
    {
        const float f = std::clamp(float_staging[i], -1.0f, 1.0f);
        int_staging[i] = static_cast<int16_t>(f * 32767.0f);
    }

    // Step 3: push to libretro.
    const size_t accepted = cb(int_staging, available);

    // Step 4: stash any unaccepted tail for the next drain.
    if (accepted < available)
    {
        const u32 remaining = static_cast<u32>(available - accepted);
        std::memcpy(m_pending_buffer, int_staging + accepted * 2,
                    remaining * 2 * sizeof(int16_t));
        m_pending_frames = remaining;
    }

    if (!m_first_drain_logged)
    {
        FrontendLog(RETRO_LOG_INFO,
            "LibretroAudioStream first drain: %u frames pushed (frontend accepted %zu)",
            available, accepted);
        m_first_drain_logged = true;
    }

    return static_cast<u32>(accepted);
}
```

- [ ] **Step 4.2: Build**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && cmake --build build --target pcsx2_libretro -j8
```

Expected: build succeeds. If you get an error about `GetBufferedFramesRelaxed` not being public, re-check `pcsx2/Host/AudioStream.h:72` — it should already be public.

- [ ] **Step 4.3: Commit**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && git add pcsx2-libretro/LibretroAudioStream.cpp && git commit -m "SP4 step 4: DrainToLibretroCallback — real implementation

Reads from the ring buffer, converts float to int16 stereo, pushes
via cb. Re-queues any unaccepted tail for the next drain (libretro
spec allows partial consumption). One-shot diagnostic log on first
drain."
```

---

## Task 5 — Switch Settings.cpp from `nullout` to `Backend = Libretro`

Activates the new backend. Also forces stereo (`expansion_mode = Disabled`) since libretro batch callback is stereo-only.

**Files:**
- Modify: `pcsx2-master/pcsx2-libretro/Settings.cpp:158-159`

- [ ] **Step 5.1: Replace the SPU2 nullout setting**

In `pcsx2-master/pcsx2-libretro/Settings.cpp`, find:

```cpp
    // Disable hardware audio output — SPU2 still initializes but discards.
    g_si.SetStringValue("SPU2/Output", "OutputModule", "nullout");
```

Replace with:

```cpp
    // SP4: route SPU2 → retro_audio_sample_batch_t via LibretroAudioStream.
    // The string-form key in the INI section is the AudioBackend name
    // matching s_backend_names in pcsx2/Host/AudioStream.cpp; "Libretro"
    // is the SP4 enum addition.
    g_si.SetStringValue("SPU2", "Backend", "Libretro");

    // Libretro's batch callback is stereo only. Forcing expansion off
    // avoids LibretroAudioStream's pxAssert(expansion_mode == Disabled).
    g_si.SetStringValue("SPU2", "ExpansionMode", "Disabled");
```

(If you see compile errors about the section name "SPU2/Output" being elsewhere, search Settings.cpp for any other SPU2-related calls and apply the same pattern. The other place might be `SPU2`. Verify by searching `pcsx2/SPU2/` for `LoadSave` to confirm the section name.)

- [ ] **Step 5.2: Verify the section name is correct**

Quick check before building:

```sh
grep -n "wrap.GetSection\|wrap_section\|ssection" "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2/SPU2/spu2.cpp" | head -5
grep -nE "SectionInfo|Section.*=.*\"SPU2" "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/pcsx2/Config.h" | head -5
```

If the section isn't `"SPU2"`, adjust the calls in Step 5.1 accordingly. (The PCSX2 default has historically been `[SPU2]` in the INI.)

- [ ] **Step 5.3: Build**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && cmake --build build --target pcsx2_libretro -j8
```

Expected: build succeeds.

- [ ] **Step 5.4: Commit**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && git add pcsx2-libretro/Settings.cpp && git commit -m "SP4 step 5: switch Settings to AudioBackend::Libretro + force stereo

Replaces the SP3-era nullout with the new libretro backend. Forces
expansion off so LibretroAudioStream's stereo invariant holds."
```

---

## Task 6 — Drain audio in `retro_run` after present_cv signals

The final wire: each video frame, drain whatever's in the ring buffer and push to libretro.

**Files:**
- Modify: `pcsx2-master/pcsx2-libretro/LibretroFrontend.cpp:164-194`

- [ ] **Step 6.1: Add the include**

Near the top of `pcsx2-master/pcsx2-libretro/LibretroFrontend.cpp` (after the existing `#include "LibretroFrontend.h"` line), add:

```cpp
#include "LibretroAudioStream.h"
```

- [ ] **Step 6.2: Add the drain call to retro_run**

In `pcsx2-master/pcsx2-libretro/LibretroFrontend.cpp`, find the end of `retro_run` (around line 193):

```cpp
    g_present_cv.wait_for(lock, 100ms, [] { return g_present_ready.load(); });
    g_present_ready.store(false, std::memory_order_release);
}
```

Replace with:

```cpp
    g_present_cv.wait_for(lock, 100ms, [] { return g_present_ready.load(); });
    g_present_ready.store(false, std::memory_order_release);
    lock.unlock(); // release before draining; drain doesn't touch g_present_*

    // SP4: drain SPU2 output. ActiveStream() is null pre-VM-init or post-
    // shutdown; audio_batch_cb is null until the frontend has called
    // retro_set_audio_sample_batch. Both are common at startup, so silently
    // skip if either is missing.
    if (auto* stream = Pcsx2Libretro::LibretroAudioStream::ActiveStream())
    {
        if (g_frontend.audio_batch_cb)
        {
            // Drain everything currently buffered, capped at MAX_FRAMES_PER_DRAIN
            // (2048 stereo frames = 42 ms @ 48 kHz, comfortably bounds one frame's
            // worth of audio at any reasonable host fps).
            stream->DrainToLibretroCallback(g_frontend.audio_batch_cb, 2048);
        }
    }
}
```

- [ ] **Step 6.3: Build**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && cmake --build build --target pcsx2_libretro -j8
```

Expected: build succeeds.

- [ ] **Step 6.4: Commit**

```sh
cd "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master" && git add pcsx2-libretro/LibretroFrontend.cpp && git commit -m "SP4 step 6: drain audio in retro_run after present_cv

Pulls accumulated SPU2 frames after each video frame signal and
pushes them through the cached libretro batch callback. Skips
cleanly when stream or callback is unset."
```

---

## Task 7 — End-to-end smoke test on Ratchet & Clank

The build is complete. Now we verify it actually plays audio in RetroNest.

**Files:** none (manual test).

- [ ] **Step 7.1: Copy the new dylib into RetroNest's cores directory**

```sh
cp "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/build/pcsx2-libretro/pcsx2_libretro.dylib" ~/Documents/RetroNest/emulators/libretro/cores/
```

- [ ] **Step 7.2: Launch RetroNest, start Ratchet & Clank**

Start RetroNest. Navigate to Ratchet & Clank in the game list. Launch.

Expected:
- Within ~10 seconds: PCSX2 BIOS boot logo or game title screen renders (SP3 behavior, unchanged).
- **BIOS boot chime plays audibly through the host audio device.**
- After title screen: in-game music + SFX play in sync with video.

If silent: check the RetroNest log file for the `LibretroAudioStream first drain: N frames pushed` line. If it never appears, `s_active_stream` was never set — likely `Settings.cpp` didn't take effect (verify Step 5 wrote to the right section). If it appears with `frontend accepted 0`, the libretro frontend isn't pulling — check RetroNest's audio backend selection.

- [ ] **Step 7.3: Verify pause/resume**

In-game, press **Cmd+Shift+Escape** to open the in-game menu.

Expected: audio stops cleanly within ~one frame. No looping last-frame buzz.

Close the menu. Audio resumes.

- [ ] **Step 7.4: Verify save state round-trip**

In-game menu → save state. Continue playing for ~30 seconds. In-game menu → load state.

Expected: VM resumes audio from the saved point. No corruption (no static, no chipmunked playback).

- [ ] **Step 7.5: Verify lifecycle (load → unload → load again)**

Exit Ratchet & Clank back to the game list. Re-launch the same game.

Expected: audio works on the second launch. (Verifies `~LibretroAudioStream` cleared `s_active_stream` correctly.)

- [ ] **Step 7.6: (Optional) Verify SoundTouch stretch under fast-forward**

If RetroNest exposes a turbo / fast-forward hotkey, hold it.

Expected: audio time-stretches — faster, preserved pitch. **Not chipmunked** (which would mean PCSX2 is just resampling to a higher rate without SoundTouch). If chipmunked, the `SetStretchEnabled(true)` path in `CreateLibretroAudioStream` isn't being invoked — verify `EmuConfig.SPU2.IsTimeStretchEnabled()` returns true under your config.

- [ ] **Step 7.7: If all four mandatory checks (7.2, 7.3, 7.4, 7.5) pass, declare SP4 functionally shipped**

Mark this task complete only after all four.

---

## Task 8 — Update project memory + write SP4 session-handoff memory

Bookkeeping. SP3.5's memory model is the template.

**Files:**
- Modify: `~/.claude/projects/-Users-mark-Documents-Projects-Pcsx2-Experiment-/memory/project_pcsx2_libretro_port.md`
- Modify: `~/.claude/projects/-Users-mark-Documents-Projects-Pcsx2-Experiment-/memory/MEMORY.md`
- Replace (or supersede): `~/.claude/projects/-Users-mark-Documents-Projects-Pcsx2-Experiment-/memory/session_handoff_sp3_5_shipped.md` → rename concept to `session_handoff_sp4_shipped.md`

- [ ] **Step 8.1: Update the project sub-projects list**

In `project_pcsx2_libretro_port.md`, change item 5 in the sub-projects list from:

```markdown
5. ⏳ **Audio output (SP4)** — SPU2 → retro_audio_sample_batch_t. Next.
```

to:

```markdown
5. ✅ **Audio output (SP4)** — DONE. Spec/plan `2026-05-11-pcsx2-libretro-audio-output*`. LibretroAudioStream subclass routes SPU2 → retro_audio_sample_batch_t with full SoundTouch stretch retained. Stereo-only (5.1/7.1 expansion forced off — libretro batch callback is stereo). Sample rate hardcoded 48000. First sub-project to touch upstream files outside the CMakeLists block: 5 lines across `pcsx2/Host/AudioStreamTypes.h` + `AudioStream.cpp` (comment-flagged for rebase).
```

- [ ] **Step 8.2: Add a discipline-exception note to the architecture facts section of project memory**

In `project_pcsx2_libretro_port.md`'s "Architecture facts" section, add a new bullet under "Discipline":

```markdown
- **Audio backend exception (SP4).** PCSX2's audio integration point is the `AudioStream` class with a static factory baked into libPCSX2 — there is no `Host::` hook. SP4 introduces `AudioBackend::Libretro` plus a dispatch case in `pcsx2/Host/AudioStreamTypes.h` and `pcsx2/Host/AudioStream.cpp` (5 lines, all comment-flagged). This is the only sanctioned exception to the no-upstream-edits rule. Future sub-projects must not widen it without an explicit brainstorm + spec note.
```

- [ ] **Step 8.3: Write the SP4 session handoff memory**

Create `~/.claude/projects/-Users-mark-Documents-Projects-Pcsx2-Experiment-/memory/session_handoff_sp4_shipped.md`:

```markdown
---
name: SP4 shipped — next session handoff
description: Where SP4 audio left off and what to pick up next. Audio routing complete; SP5 (input) is the next sub-project.
type: project
---
## Status when this session ended ([fill in date])

**SP4 (libretro audio output) shipped.** Ratchet & Clank plays audio in sync with video through RetroNest's libretro audio path. SoundTouch stretch retained, stereo only, 48 kHz hardcoded. Five lines of upstream-file deviation in `pcsx2/Host/` (comment-flagged); see project memory for the discipline exception.

## Commits added during the SP4 session

On pcsx2-master `retronest-libretro`:
- (commit hash) SP4 step 1: AudioBackend::Libretro enum + factory dispatch
- (commit hash) SP4 step 2: LibretroAudioStream.h
- (commit hash) SP4 step 3: LibretroAudioStream skeleton + CMake wire-up
- (commit hash) SP4 step 4: DrainToLibretroCallback real implementation
- (commit hash) SP4 step 5: switch Settings to AudioBackend::Libretro
- (commit hash) SP4 step 6: drain audio in retro_run

On RetroNest `main`:
- (commit hash) SP4 spec
- (commit hash) SP4 implementation plan

## Where to pick up

If continuing the libretro port: **SP5 (input)** is the next sub-project. Wire `retro_input_state_t` → PAD plugin. Re-enable the input source disables in Settings.cpp (currently `SDL=false`, `XInput=false`, `DInput=false`, `RawInput=false`). Mirror the gsrunner Main.cpp input-callback shape.

If the user wants to go back to SP3.6 (shutdown crash) — still parked. SP4 didn't unblock it.

## Known limitations carried forward

- Stereo only.
- 48 kHz hardcoded — PSX titles play slightly off-pitch.
- SP3.6 (Quit crash) still open — workaround is force-quit RetroNest from the dock.
```

(Substitute real commit hashes after each task lands.)

- [ ] **Step 8.4: Update the MEMORY.md index entry**

In `~/.claude/projects/-Users-mark-Documents-Projects-Pcsx2-Experiment-/memory/MEMORY.md`, replace the SP3.5 handoff line:

```markdown
- [SP3.5 shipped — next session handoff](session_handoff_sp3_5_shipped.md) — Where SP3.5 ended (overlays shipped, Quit crash deferred to SP3.6), what smoke tests are still owed, where to pick up
```

with:

```markdown
- [SP4 shipped — next session handoff](session_handoff_sp4_shipped.md) — Where SP4 ended (audio output shipped), where to pick up (SP5 input)
```

(If the SP3.5 smoke tests still aren't confirmed when SP4 ships, **don't** delete the SP3.5 handoff file yet — keep both index entries until SP3.5 is truly closed.)

- [ ] **Step 8.5: No commit (these are user-local memory files, not in any git repo)**

Memory files live outside source control. Save and move on.

---

## Self-Review (writing-plans skill required)

After writing this plan, did the spec coverage check:

- ✅ Spec §"Architecture · file layout" — Tasks 1–6 cover every file in the table.
- ✅ Spec §"Lifecycle" table — Task 5 (Settings → SPU2::CreateOutputStream) + Task 7.5 (load → unload → load again) cover ctor/dtor singleton flow.
- ✅ Spec §"Error handling" — Task 4 implements `audio_batch_cb` null-skip, partial-consumption re-queue, ring underrun (returns 0). Task 6 wraps with `ActiveStream()` null-check.
- ✅ Spec §"Sample format conversion" — Task 4 implements float→int16 with clamp.
- ✅ Spec §"Verification (testing strategy)" 7-item list — Task 7 covers items 1–6; the diagnostic log line (item 7) is in Task 4.
- ✅ Spec §"Discipline note" — Task 8.2 updates project memory with the exception.

Type / signature consistency:
- `DrainToLibretroCallback` — `u32` return, `(retro_audio_sample_batch_t cb, u32 max_frames)` everywhere ✓
- `ActiveStream` — `static LibretroAudioStream*` returning `s_active_stream.load()` ✓
- `MAX_FRAMES_PER_DRAIN` — `2048` everywhere (header constant + Task 6's hardcoded `2048` matches; consider making Task 6 use `Pcsx2Libretro::LibretroAudioStream::MAX_FRAMES_PER_DRAIN` if visibility allows — currently private so `2048` is duplicated. Acceptable; documented inline.)
- `CreateLibretroAudioStream` — out-of-namespace free function, signature matches `extern` decl in `AudioStream.cpp` ✓

No placeholders, no TBDs, every code block is complete.

---

## Plan complete

Plan saved. Two execution options:

**1. Subagent-Driven (recommended)** — Fresh subagent per task, review between tasks, fast iteration. Good for SP4 because each task is short and the build is the natural checkpoint.

**2. Inline Execution** — Execute tasks in this session, batch with checkpoints. Good if you want to watch the build output live.

Which approach?
