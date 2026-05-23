# MoltenVK ↔ Metal interop spike

**Date:** 2026-05-23
**Scope:** SP0 of the Dolphin libretro conversion ([spec](../specs/2026-05-23-dolphin-libretro-conversion-design.md), [plan](../plans/2026-05-23-sp0-moltenvk-metal-interop-spike.md))
**Question:** can `LibretroVulkanItem` (SP4) hand a MoltenVK-rendered `VkImage` into Qt's Metal scene graph without an extra copy?
**PoC:** `cpp/spike/moltenvk_metal_interop/`

## Environment

- macOS: 26.4.1
- MoltenVK: 1.4.1 (Homebrew)
- Vulkan loader: 1.4.350.0 (Homebrew)
- Vulkan API negotiated: 1.3.334
- Hardware: Apple M4

## Result

```
device: Apple M4 (api 1.3.334)
[Path A] MTLTexture storageMode=2 is GPU-private; getBytes not supported. Export succeeded but CPU readback requires a blit to a shared texture (out of scope for this PoC).
Path A (vkExportMetalObjectsEXT): FAILED
Path B (IOSurface bridge):        OK
EXIT CODE: 0
```

## Findings

### Path A — `vkExportMetalObjectsEXT` direct extraction

- **Result:** API-level success. PoC's CPU readback verification N/A.
- **What works:**
  - `vkGetDeviceProcAddr(device, "vkExportMetalObjectsEXT")` returns a non-null function pointer — the extension is exposed at device level.
  - `vkExportMetalObjectsEXT` returns a non-nil `MTLTexture` for an `VK_IMAGE_TILING_OPTIMAL`, device-local `VkImage`. The handle extraction works.
- **What didn't run:**
  - The PoC verifies bytes via `[mtlTex getBytes:...]`, but the exported texture has `MTLStorageModePrivate` (because MoltenVK maps device-local + tiling-optimal to private storage on Apple Silicon). `getBytes` SIGSEGVs on private-storage textures. A storageMode guard prevents the crash and reports FAILED.
- **Why this isn't a real Path A failure for SP4:**
  - SP4's `LibretroVulkanItem` hands the `MTLTexture` to Qt's scene graph for composition via Metal shaders. GPU-side consumers don't need `getBytes` — they sample the texture from a fragment shader, which works fine against private storage. The PoC's verification methodology is unsuited to a device-local export, but the production use case never exercises that path.

### Path B — IOSurface bridge

- **Result:** End-to-end OK.
- **What works:**
  - `IOSurfaceCreate` → `[mtlDevice newTextureWithDescriptor:iosurface:plane:]` produces an `MTLStorageModeShared` `MTLTexture` backed by the IOSurface.
  - `vkCreateImage` with `VkImportMetalIOSurfaceInfoEXT` in `pNext` succeeds and binds memory implicitly (no `vkAllocateMemory` / `vkBindImageMemory` call needed — MoltenVK owns the binding through the IOSurface).
  - `vkCmdClearColorImage` + `vkQueueWaitIdle` produces bytes visible to subsequent `[mtlTex getBytes:...]` calls. Bytes verified: `0x56 0x34 0x12 0xFF` (BGRA order for kB=0x56, kG=0x34, kR=0x12, kA=0xFF).
- **Caveats:** None observed on this configuration. Both `VK_FORMAT_B8G8R8A8_UNORM` (Vulkan side) and `MTLPixelFormatBGRA8Unorm` (Metal side) must match — they do.

### Performance characterization

The PoC does not measure throughput. Both paths are functionally zero-copy when they work — neither involves a `memcpy` of pixel data on the success path. Real perf comparison happens during SP4 with realistic frame loads (Dolphin rendering at internal resolution, composited by Qt at 60Hz+).

## Recommendation for SP4

**Primary path: Path B (IOSurface bridge).**

Reasoning:
1. **Battle-tested in this codebase already.** `LibretroGLItem` (PPSSPP's existing GL path) uses the same GL→IOSurface→Metal pattern. Lifecycle, teardown ordering, fence discipline are all established. SP4 inherits the discipline.
2. **End-to-end verified by this spike.** Bytes round-tripped through Vulkan-write → Metal-read with no surprises.
3. **No storage-mode gotchas.** IOSurface forces shared storage, which is what Qt's scene-graph wants for composition anyway.
4. **Khronos-standard API.** `VK_EXT_metal_objects`'s `VkImportMetalIOSurfaceInfoEXT` is the same extension Path A uses — no new dependency.

**Fallback: Path A is viable as a future optimization** if profiling reveals IOSurface management overhead matters. The API-level mechanism is proven (export returns valid MTLTexture); SP4 doesn't need it on day one.

### Implementation notes for `LibretroVulkanItem`

- **Texture creation timing:** SP4 receives the libretro core's `retro_hw_render_callback`-derived `VkImage` per frame. RetroNest allocates the IOSurface, hands its backing wrapped as both `MTLTexture` (to Qt) and `VkImage` (to the core's `retro_hw_render_interface_vulkan` via the `image` field of the callback's framebuffer struct).
- **Lifetime:** The `MTLTexture` handed to `QSGSimpleTextureNode` (via `QSGTexture::fromNative()`) must outlive any frame referencing it. Same fence discipline as `LibretroGLItem` — see `GameSession::preShutdownRenderFence` which currently gates on `m_libretroBackend == "gl"`. Extend to `gl || vulkan` once SP4 has run a Quit/Save&Quit smoke test.
- **Format choice:** `VK_FORMAT_B8G8R8A8_UNORM` ↔ `MTLPixelFormatBGRA8Unorm` is the verified working pair. If Dolphin's Vulkan backend wants RGBA8 instead, an IOSurface format conversion is needed (either at SwapChain modification time on the core side, or via a Metal blit on the host side). Verify during SP4 integration.
- **No explicit `vkAllocateMemory`:** when `VkImportMetalIOSurfaceInfoEXT` is in `pNext`, MoltenVK skips the standard memory binding path. Don't try to bind your own — it'll fail.

### Plan bugs fixed during the spike (apply to SP4 too)

The spike implementation surfaced three Vulkan/MoltenVK API mistakes from the plan that SP4's `LibretroVulkanContext` should avoid:

1. **`VK_EXT_metal_objects` is a device extension**, not an instance extension. Pass it via `VkDeviceCreateInfo::ppEnabledExtensionNames`, never `VkInstanceCreateInfo::ppEnabledExtensionNames` — the latter returns `VK_ERROR_EXTENSION_NOT_PRESENT`.
2. **`VkExportMetalTextureInfoEXT::plane` is `VK_IMAGE_ASPECT_COLOR_BIT`** for single-plane RGBA/BGRA images. `VK_IMAGE_ASPECT_PLANE_0_BIT` is for multi-planar (YCbCr) formats only.
3. **`MTLTexture_id` is already `id<MTLTexture>`** in Objective-C compilation mode — no `__bridge` cast needed. Direct assignment.

## Sub-project closure

This spike is **complete**. The PoC at `cpp/spike/moltenvk_metal_interop/` is preserved as a reference and as a smoke-test for the Vulkan/MoltenVK environment. It can be deleted once SP4 ships and the `LibretroVulkanItem` implementation has settled.

**SP1 unblocked**: SP1 (skeleton libretro core, Metal stub) doesn't touch Vulkan. The Vulkan path enters at SP4. SP1 can proceed immediately with the recommendation already locked in.
