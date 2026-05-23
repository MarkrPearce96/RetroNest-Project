# SP0: MoltenVK / Metal Interop Spike Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Settle the open question from the Dolphin libretro conversion spec: can `LibretroVulkanItem` import a MoltenVK-rendered `VkImage` directly as a `MTLTexture`, or must we use an IOSurface bridge? Produce a written spike note with the answer and a working proof-of-concept.

**Architecture:** 1-day timeboxed exploration. One small standalone Objective-C++ PoC at `RetroNest-Project/cpp/spike/moltenvk_metal_interop/`, exercising both candidate paths against a known render-then-sample test. Spike note at `RetroNest-Project/docs/superpowers/notes/`. No production code touched. No RetroNest-side architecture decisions cascade from this — SP4 will read the spike note and pick the path.

**Tech Stack:** MoltenVK, Vulkan 1.3, Metal 3, CoreVideo (IOSurface), Objective-C++, CMake. macOS Darwin 25.x (per `uname`).

**Parent spec:** `docs/superpowers/specs/2026-05-23-dolphin-libretro-conversion-design.md`

**Two paths under test:**

- **Path A — `VK_EXT_metal_objects` direct extraction.** Render into a `VkImage` via MoltenVK, then call `vkExportMetalObjectsEXT` to retrieve the underlying `id<MTLTexture>`. Zero-copy if it works.
- **Path B — IOSurface bridge.** Allocate an `IOSurfaceRef`, wrap it as both an `MTLTexture` (`newTextureWithDescriptor:iosurface:plane:`) and a `VkImage` (via `MVK_KHR_external_memory_metal` or `vkUseIOSurfaceMVK`). Render in Vulkan, sample in Metal — both see the same IOSurface bytes. Always works; the question is whether it adds a copy in practice.

**Success criteria:** The PoC prints, for each path, either `OK: rendered <color>, sampled <color>` or `FAILED: <reason>`. The spike note translates that into a recommendation for SP4.

---

### Task 1: Locate MoltenVK and Vulkan SDK on the build machine

**Files:** None modified. This is investigation.

- [ ] **Step 1: Check whether the Vulkan SDK / MoltenVK is already installed**

Run:
```bash
echo "VULKAN_SDK=${VULKAN_SDK:-<unset>}"
ls -l /usr/local/lib/libMoltenVK.dylib 2>/dev/null
ls -l /usr/local/lib/libvulkan.dylib 2>/dev/null
ls -l "$HOME/VulkanSDK/" 2>/dev/null
brew list molten-vk 2>/dev/null
brew list vulkan-headers 2>/dev/null
brew list vulkan-loader 2>/dev/null
```

Expected: at least one of these resolves. If nothing resolves, proceed to Step 2.

- [ ] **Step 2: If nothing installed, install via Homebrew**

Run:
```bash
brew install molten-vk vulkan-headers vulkan-loader
```

Expected: clean install. Re-run Step 1 commands to verify.

- [ ] **Step 3: Record the locations**

Determine and note:
- `MOLTENVK_DYLIB` — absolute path to `libMoltenVK.dylib`
- `VULKAN_HEADERS` — directory containing `vulkan/vulkan.h`
- `VULKAN_LOADER_DYLIB` — absolute path to `libvulkan.dylib` (loader)

These values get baked into the CMakeLists in Task 2.

- [ ] **Step 4: Verify `VK_EXT_metal_objects` is exposed**

Run:
```bash
strings "$MOLTENVK_DYLIB" | grep -E "VK_EXT_metal_objects|vkExportMetalObjects"
```

Expected: matches found (Path A is potentially viable). If no matches, Path A is unavailable on this MoltenVK build — record that, the spike will conclude in favor of Path B after Task 4 confirms it.

- [ ] **Step 5: Commit nothing yet**

No files have changed. Move to Task 2.

---

### Task 2: Create the spike scaffolding

**Files:**
- Create: `cpp/spike/moltenvk_metal_interop/CMakeLists.txt`
- Create: `cpp/spike/moltenvk_metal_interop/main.mm`
- Create: `cpp/spike/moltenvk_metal_interop/README.md`

- [ ] **Step 1: Create the directory**

Run:
```bash
mkdir -p /Users/mark/Documents/Projects/RetroNest-Project/cpp/spike/moltenvk_metal_interop
```

- [ ] **Step 2: Write the CMakeLists**

Create `cpp/spike/moltenvk_metal_interop/CMakeLists.txt`:

```cmake
# SP0 spike — MoltenVK ↔ Metal texture-sharing PoC.
# Standalone target outside the main RetroNest build. Build with:
#   cd cpp/spike/moltenvk_metal_interop
#   cmake -B build -DMOLTENVK_DYLIB=/path/to/libMoltenVK.dylib \
#                  -DVULKAN_HEADERS=/path/to/vulkan/include \
#                  -DVULKAN_LOADER_DYLIB=/path/to/libvulkan.dylib
#   cmake --build build
#   ./build/moltenvk_metal_interop_spike
cmake_minimum_required(VERSION 3.20)
project(moltenvk_metal_interop_spike LANGUAGES OBJCXX C)

set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "")
set(CMAKE_CXX_STANDARD 20)

if(NOT MOLTENVK_DYLIB OR NOT VULKAN_HEADERS OR NOT VULKAN_LOADER_DYLIB)
    message(FATAL_ERROR
        "Set -DMOLTENVK_DYLIB, -DVULKAN_HEADERS, -DVULKAN_LOADER_DYLIB at configure time.")
endif()

add_executable(moltenvk_metal_interop_spike main.mm)

target_include_directories(moltenvk_metal_interop_spike PRIVATE "${VULKAN_HEADERS}")
target_link_libraries(moltenvk_metal_interop_spike PRIVATE
    "${VULKAN_LOADER_DYLIB}"
    "-framework Foundation"
    "-framework Metal"
    "-framework QuartzCore"
    "-framework IOSurface"
    "-framework CoreVideo"
)
target_compile_options(moltenvk_metal_interop_spike PRIVATE -fobjc-arc -Wall -Wextra)
```

- [ ] **Step 3: Write the README**

Create `cpp/spike/moltenvk_metal_interop/README.md`:

```markdown
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
```

- [ ] **Step 4: Write the main.mm skeleton**

Create `cpp/spike/moltenvk_metal_interop/main.mm`:

```objective-c++
// SP0 spike — MoltenVK ↔ Metal interop PoC.
// Tests two candidate paths for sharing a texture between MoltenVK and Metal:
//   Path A: VK_EXT_metal_objects — vkExportMetalObjectsEXT extracts MTLTexture.
//   Path B: IOSurface bridge — allocate IOSurfaceRef, wrap as both VkImage + MTLTexture.

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <IOSurface/IOSurface.h>
#define VK_USE_PLATFORM_METAL_EXT
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

static bool runPathA(VkInstance instance, VkPhysicalDevice phys, VkDevice device);
static bool runPathB(VkInstance instance, VkPhysicalDevice phys, VkDevice device);

int main() {
    // Step T3 fills this in: create VkInstance / VkPhysicalDevice / VkDevice via MoltenVK.
    fprintf(stderr, "spike: not yet implemented\n");
    return 1;
}
```

- [ ] **Step 5: Verify the skeleton builds**

Run (substituting the paths recorded in Task 1):
```bash
cd cpp/spike/moltenvk_metal_interop
cmake -B build \
    -DMOLTENVK_DYLIB="$MOLTENVK_DYLIB" \
    -DVULKAN_HEADERS="$VULKAN_HEADERS" \
    -DVULKAN_LOADER_DYLIB="$VULKAN_LOADER_DYLIB"
cmake --build build 2>&1 | tail -20
```

Expected: clean build of `moltenvk_metal_interop_spike` executable. Running it prints "spike: not yet implemented" and exits 1.

- [ ] **Step 6: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add cpp/spike/moltenvk_metal_interop/
git commit -m "SP0 scaffold: MoltenVK/Metal interop spike skeleton"
```

---

### Task 3: Implement Vulkan/MoltenVK initialization

**Files:**
- Modify: `cpp/spike/moltenvk_metal_interop/main.mm`

Bring up a minimal Vulkan instance + device backed by MoltenVK. No window, no swapchain — we just need a `VkDevice` to allocate `VkImage`s against.

- [ ] **Step 1: Replace `main()` with the init sequence**

Replace the `main()` body in `main.mm`:

```objective-c++
int main() {
    // ---- VkInstance ----
    const char* instanceExts[] = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        VK_EXT_METAL_OBJECTS_EXTENSION_NAME,  // best-effort; not all loaders expose
    };
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_3;
    app.pApplicationName = "moltenvk_metal_interop_spike";

    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = sizeof(instanceExts) / sizeof(*instanceExts);
    ici.ppEnabledExtensionNames = instanceExts;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&ici, nullptr, &instance);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "vkCreateInstance failed: %d\n", r);
        return 2;
    }

    // ---- VkPhysicalDevice (pick first) ----
    uint32_t physCount = 0;
    vkEnumeratePhysicalDevices(instance, &physCount, nullptr);
    if (physCount == 0) {
        fprintf(stderr, "no Vulkan physical devices\n");
        return 3;
    }
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    vkEnumeratePhysicalDevices(instance, &physCount, &phys);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(phys, &props);
    fprintf(stderr, "device: %s (api %u.%u.%u)\n",
            props.deviceName,
            VK_VERSION_MAJOR(props.apiVersion),
            VK_VERSION_MINOR(props.apiVersion),
            VK_VERSION_PATCH(props.apiVersion));

    // ---- Queue family ----
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qCount, qProps.data());
    uint32_t gfxQ = UINT32_MAX;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { gfxQ = i; break; }
    }
    if (gfxQ == UINT32_MAX) { fprintf(stderr, "no gfx queue\n"); return 4; }

    // ---- VkDevice with VK_EXT_metal_objects when available ----
    const char* devExts[] = { VK_EXT_METAL_OBJECTS_EXTENSION_NAME };
    float qPriority = 1.0f;
    VkDeviceQueueCreateInfo dqci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    dqci.queueFamilyIndex = gfxQ;
    dqci.queueCount = 1;
    dqci.pQueuePriorities = &qPriority;
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &dqci;
    dci.enabledExtensionCount = sizeof(devExts) / sizeof(*devExts);
    dci.ppEnabledExtensionNames = devExts;

    VkDevice device = VK_NULL_HANDLE;
    r = vkCreateDevice(phys, &dci, nullptr, &device);
    if (r != VK_SUCCESS) {
        fprintf(stderr, "vkCreateDevice failed: %d (Path A unavailable, may still try Path B)\n", r);
        // Retry without VK_EXT_metal_objects for Path B-only run.
        dci.enabledExtensionCount = 0;
        dci.ppEnabledExtensionNames = nullptr;
        r = vkCreateDevice(phys, &dci, nullptr, &device);
        if (r != VK_SUCCESS) {
            fprintf(stderr, "vkCreateDevice (fallback) failed: %d\n", r);
            return 5;
        }
    }

    bool pathAOk = runPathA(instance, phys, device);
    bool pathBOk = runPathB(instance, phys, device);

    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    fprintf(stdout, "Path A (vkExportMetalObjectsEXT): %s\n", pathAOk ? "OK" : "FAILED");
    fprintf(stdout, "Path B (IOSurface bridge):        %s\n", pathBOk ? "OK" : "FAILED");
    return (pathAOk || pathBOk) ? 0 : 6;
}

// Stub implementations — Tasks 4 and 5 fill these in.
static bool runPathA(VkInstance, VkPhysicalDevice, VkDevice) {
    fprintf(stderr, "[Path A] not yet implemented\n");
    return false;
}
static bool runPathB(VkInstance, VkPhysicalDevice, VkDevice) {
    fprintf(stderr, "[Path B] not yet implemented\n");
    return false;
}
```

- [ ] **Step 2: Build and run**

Run:
```bash
cd cpp/spike/moltenvk_metal_interop
cmake --build build 2>&1 | tail -10
./build/moltenvk_metal_interop_spike
```

Expected: stderr prints `device: <name>` plus `[Path A] not yet implemented` and `[Path B] not yet implemented`. stdout prints both as `FAILED`. Exit code 6.

If the device name is empty or `vkCreateInstance` fails: MoltenVK loader configuration is wrong. Check `VK_ICD_FILENAMES` env var; on Homebrew it usually needs `export VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/MoltenVK_icd.json`.

- [ ] **Step 3: Commit**

```bash
git add cpp/spike/moltenvk_metal_interop/main.mm
git commit -m "SP0: Vulkan/MoltenVK init bring-up"
```

---

### Task 4: Implement Path A — `vkExportMetalObjectsEXT` direct extraction

**Files:**
- Modify: `cpp/spike/moltenvk_metal_interop/main.mm`

Render a solid color into a `VkImage`, extract the underlying `MTLTexture` via `vkExportMetalObjectsEXT`, sample it from Metal, compare bytes.

- [ ] **Step 1: Replace `runPathA` stub with the real implementation**

Replace the `runPathA` stub in `main.mm`:

```objective-c++
static bool runPathA(VkInstance instance, VkPhysicalDevice phys, VkDevice device) {
    // Resolve vkExportMetalObjectsEXT — bail if not present.
    auto pfnExport = (PFN_vkExportMetalObjectsEXT)
        vkGetDeviceProcAddr(device, "vkExportMetalObjectsEXT");
    if (!pfnExport) {
        fprintf(stderr, "[Path A] vkExportMetalObjectsEXT not present — skipping\n");
        return false;
    }

    constexpr uint32_t W = 64, H = 64;
    constexpr uint8_t kR = 0xAB, kG = 0xCD, kB = 0xEF, kA = 0xFF;

    // 1) Create VkImage (color-attachment + sampled, exportable to Metal).
    VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = {W, H, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(device, &ici, nullptr, &image) != VK_SUCCESS) {
        fprintf(stderr, "[Path A] vkCreateImage failed\n");
        return false;
    }

    // 2) Allocate + bind memory (any device-local heap).
    VkMemoryRequirements mreq;
    vkGetImageMemoryRequirements(device, image, &mreq);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    uint32_t memType = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((mreq.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memType = i; break;
        }
    }
    if (memType == UINT32_MAX) {
        fprintf(stderr, "[Path A] no device-local memory type\n");
        vkDestroyImage(device, image, nullptr);
        return false;
    }
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mreq.size;
    mai.memoryTypeIndex = memType;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(device, &mai, nullptr, &mem) != VK_SUCCESS ||
        vkBindImageMemory(device, image, mem, 0) != VK_SUCCESS) {
        fprintf(stderr, "[Path A] memory alloc/bind failed\n");
        vkDestroyImage(device, image, nullptr);
        return false;
    }

    // 3) Clear the image to (kR,kG,kB,kA) via a one-shot command buffer.
    uint32_t gfxQ = 0;
    VkQueue queue; vkGetDeviceQueue(device, gfxQ, 0, &queue);
    VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = gfxQ;
    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(device, &cpi, nullptr, &pool);
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = pool; cbai.commandBufferCount = 1;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cb; vkAllocateCommandBuffers(device, &cbai, &cb);

    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &cbbi);

    auto barrier = [&](VkImageLayout oldL, VkImageLayout newL,
                       VkAccessFlags srcA, VkAccessFlags dstA,
                       VkPipelineStageFlags srcS, VkPipelineStageFlags dstS) {
        VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = oldL; b.newLayout = newL;
        b.srcAccessMask = srcA; b.dstAccessMask = dstA;
        b.image = image;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vkCmdPipelineBarrier(cb, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
    };

    barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkClearColorValue clear = {{ kR / 255.0f, kG / 255.0f, kB / 255.0f, kA / 255.0f }};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);

    barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    vkEndCommandBuffer(cb);

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cb;
    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // 4) Export the underlying MTLTexture via VK_EXT_metal_objects.
    VkExportMetalTextureInfoEXT tinfo = {VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT};
    tinfo.image = image;
    tinfo.plane = VK_IMAGE_ASPECT_PLANE_0_BIT;  // single-plane RGBA: ignored, but field is required
    VkExportMetalObjectsInfoEXT info = {VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT};
    info.pNext = &tinfo;
    pfnExport(device, &info);
    id<MTLTexture> mtlTex = (__bridge id<MTLTexture>)tinfo.mtlTexture;
    if (!mtlTex) {
        fprintf(stderr, "[Path A] export returned nil MTLTexture\n");
        vkFreeMemory(device, mem, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    // 5) Read back via Metal and verify bytes.
    uint8_t pixels[W * H * 4] = {};
    [mtlTex getBytes:pixels
         bytesPerRow:W * 4
          fromRegion:MTLRegionMake2D(0, 0, W, H)
         mipmapLevel:0];
    bool ok = pixels[0] == kR && pixels[1] == kG && pixels[2] == kB && pixels[3] == kA;
    if (!ok) {
        fprintf(stderr, "[Path A] mismatch: got %02x %02x %02x %02x, want %02x %02x %02x %02x\n",
                pixels[0], pixels[1], pixels[2], pixels[3], kR, kG, kB, kA);
    }

    vkFreeMemory(device, mem, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    return ok;
}
```

- [ ] **Step 2: Build and run**

Run:
```bash
cd cpp/spike/moltenvk_metal_interop
cmake --build build 2>&1 | tail -10
./build/moltenvk_metal_interop_spike
```

Expected one of:
- stdout: `Path A (vkExportMetalObjectsEXT): OK` → Path A works on this machine.
- stdout: `Path A (vkExportMetalObjectsEXT): FAILED` plus a stderr line explaining why (proc not present, export returned nil, byte mismatch, or vk error code).

Either outcome is a valid spike result. Record which.

- [ ] **Step 3: Commit**

```bash
git add cpp/spike/moltenvk_metal_interop/main.mm
git commit -m "SP0: Path A — vkExportMetalObjectsEXT extraction"
```

---

### Task 5: Implement Path B — IOSurface bridge

**Files:**
- Modify: `cpp/spike/moltenvk_metal_interop/main.mm`

Allocate an `IOSurfaceRef`, wrap it as both an `MTLTexture` and a `VkImage`. Render in Vulkan via clear-color, read back via Metal, verify bytes.

- [ ] **Step 1: Replace `runPathB` stub with the real implementation**

Replace the `runPathB` stub in `main.mm`:

```objective-c++
static bool runPathB(VkInstance, VkPhysicalDevice phys, VkDevice device) {
    constexpr uint32_t W = 64, H = 64;
    constexpr uint8_t kR = 0x12, kG = 0x34, kB = 0x56, kA = 0xFF;

    // 1) Create an IOSurface.
    NSDictionary* surfaceProps = @{
        (id)kIOSurfaceWidth:           @(W),
        (id)kIOSurfaceHeight:          @(H),
        (id)kIOSurfaceBytesPerElement: @(4),
        (id)kIOSurfacePixelFormat:     @((unsigned)'BGRA'),  // 0x42475241
    };
    IOSurfaceRef surface = IOSurfaceCreate((__bridge CFDictionaryRef)surfaceProps);
    if (!surface) { fprintf(stderr, "[Path B] IOSurfaceCreate failed\n"); return false; }

    // 2) Wrap as MTLTexture.
    id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
    MTLTextureDescriptor* desc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                     width:W height:H mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> mtlTex = [mtlDevice newTextureWithDescriptor:desc
                                                      iosurface:surface
                                                          plane:0];
    if (!mtlTex) {
        fprintf(stderr, "[Path B] Metal IOSurface wrap failed\n");
        CFRelease(surface);
        return false;
    }

    // 3) Wrap the same IOSurface as a VkImage via VK_EXT_metal_objects
    //    (VkImportMetalIOSurfaceInfoEXT pNext on VkImageCreateInfo).
    VkImportMetalIOSurfaceInfoEXT importInfo =
        {VK_STRUCTURE_TYPE_IMPORT_METAL_IO_SURFACE_INFO_EXT};
    importInfo.ioSurface = surface;
    VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ici.pNext = &importInfo;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_B8G8R8A8_UNORM;
    ici.extent = {W, H, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage image = VK_NULL_HANDLE;
    if (vkCreateImage(device, &ici, nullptr, &image) != VK_SUCCESS) {
        fprintf(stderr, "[Path B] vkCreateImage(IOSurface) failed — VK_EXT_metal_objects required\n");
        CFRelease(surface);
        return false;
    }
    // Memory is owned by the IOSurface — no vkAllocateMemory/vkBindImageMemory.

    // 4) Clear the image to (kR,kG,kB,kA) in Vulkan.
    uint32_t gfxQ = 0;
    VkQueue queue; vkGetDeviceQueue(device, gfxQ, 0, &queue);
    VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = gfxQ;
    VkCommandPool pool = VK_NULL_HANDLE;
    vkCreateCommandPool(device, &cpi, nullptr, &pool);
    VkCommandBufferAllocateInfo cbai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = pool; cbai.commandBufferCount = 1;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cb; vkAllocateCommandBuffers(device, &cbai, &cb);

    VkCommandBufferBeginInfo cbbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &cbbi);

    VkImageMemoryBarrier b1 = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    b1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b1.image = image;
    b1.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &b1);

    // VK_FORMAT_B8G8R8A8_UNORM order — clear values follow R,G,B,A interpretation
    // (Vulkan re-orders for the chosen format when sampling).
    VkClearColorValue clear = {{ kR / 255.0f, kG / 255.0f, kB / 255.0f, kA / 255.0f }};
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cb, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);

    VkImageMemoryBarrier b2 = b1;
    b2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    b2.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &b2);
    vkEndCommandBuffer(cb);

    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cb;
    vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // 5) Read back via the Metal texture sharing the same IOSurface.
    uint8_t pixels[W * H * 4] = {};
    [mtlTex getBytes:pixels
         bytesPerRow:W * 4
          fromRegion:MTLRegionMake2D(0, 0, W, H)
         mipmapLevel:0];
    // MTLPixelFormatBGRA8Unorm → byte order is B,G,R,A in memory.
    bool ok = pixels[0] == kB && pixels[1] == kG && pixels[2] == kR && pixels[3] == kA;
    if (!ok) {
        fprintf(stderr,
            "[Path B] mismatch: got %02x %02x %02x %02x, want %02x %02x %02x %02x (BGRA)\n",
            pixels[0], pixels[1], pixels[2], pixels[3], kB, kG, kR, kA);
    }

    vkDestroyImage(device, image, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    CFRelease(surface);
    return ok;
}
```

- [ ] **Step 2: Build and run**

Run:
```bash
cd cpp/spike/moltenvk_metal_interop
cmake --build build 2>&1 | tail -10
./build/moltenvk_metal_interop_spike
```

Expected one of:
- stdout: `Path B (IOSurface bridge): OK` → Path B works (almost certainly the case; this path is well-trodden).
- stdout: `Path B (IOSurface bridge): FAILED` plus stderr explaining why. If `vkCreateImage(IOSurface) failed — VK_EXT_metal_objects required` appears, MoltenVK on this machine doesn't expose the import path either, and we'd need a different bridge mechanism (CVPixelBuffer or manual copy).

Record the result.

- [ ] **Step 3: Commit**

```bash
git add cpp/spike/moltenvk_metal_interop/main.mm
git commit -m "SP0: Path B — IOSurface bridge"
```

---

### Task 6: Write the spike note with findings + recommendation

**Files:**
- Create: `RetroNest-Project/docs/superpowers/notes/2026-05-23-moltenvk-metal-interop-spike.md`

The note translates the PoC's output into an actionable decision for SP4.

- [ ] **Step 1: Capture the actual output**

Run:
```bash
cd cpp/spike/moltenvk_metal_interop
./build/moltenvk_metal_interop_spike 2>&1 | tee /tmp/spike-output.txt
echo "exit: $?"
```

Save the output text — it gets pasted into the spike note verbatim.

- [ ] **Step 2: Write the spike note**

Create `/Users/mark/Documents/Projects/RetroNest-Project/docs/superpowers/notes/2026-05-23-moltenvk-metal-interop-spike.md`:

```markdown
# MoltenVK ↔ Metal interop spike

**Date:** 2026-05-23
**Scope:** SP0 of the Dolphin libretro conversion ([spec](../specs/2026-05-23-dolphin-libretro-conversion-design.md))
**Question:** can `LibretroVulkanItem` (SP4) import a MoltenVK-rendered `VkImage`
into Qt's Metal scene graph without an extra copy?
**PoC:** `cpp/spike/moltenvk_metal_interop/`

## Environment

- macOS: <fill in `sw_vers -productVersion` output>
- MoltenVK: <fill in `strings $MOLTENVK_DYLIB | grep '^MoltenVK'` first line>
- Vulkan loader: <fill in path used at build time>
- Hardware: <fill in `system_profiler SPDisplaysDataType | grep 'Chipset Model' | head -1`>

## Result

```
<paste full output of ./build/moltenvk_metal_interop_spike from Step 1>
```

## Findings

### Path A — `vkExportMetalObjectsEXT` direct extraction

- **Works:** <yes / no>
- **If yes:** the underlying `id<MTLTexture>` is retrievable from a Vulkan-rendered `VkImage` zero-copy. Lifetime of the texture follows the `VkImage` — release happens via `vkDestroyImage`.
- **If no:** <reason — proc-address not exposed / export returned nil / byte mismatch / vk error N>.

### Path B — IOSurface bridge

- **Works:** <yes / no>
- **If yes:** allocating an `IOSurface`, wrapping it as both `MTLTexture` (via `newTextureWithDescriptor:iosurface:plane:`) and `VkImage` (via `VkImportMetalIOSurfaceInfoEXT`) succeeds. Both views point at the same backing bytes — Vulkan writes are visible to Metal samples after `vkQueueWaitIdle`.
- **If no:** <reason>.

### Performance characterization

The PoC does not measure throughput — both paths are functionally zero-copy
when they work (no `memcpy` of pixel data). Real perf comparison happens during
SP4 with realistic frame loads.

## Recommendation for SP4

**Primary path:** <A / B>, because <reason>.

**Fallback:** <the other path, or "none — primary is reliable and we don't need fallback complexity">.

**Implementation notes for `LibretroVulkanItem`:**

- Texture creation happens on the libretro frontend's request (`retro_hw_render_callback`).
- The `MTLTexture` handed to Qt's scene graph (`QSGSimpleTextureNode` with a `QSGTexture::fromNative()` wrapping the `MTLTexture`) must outlive any frame referencing it — same fence discipline `LibretroGLItem` uses for the GL→IOSurface→Metal bridge.
- <Path-specific note: if A, document the export-info struct's lifetime guarantee. If B, document the IOSurface ref-counting boundary between Qt and the libretro core.>

## Sub-project closure

This spike is complete. The PoC at `cpp/spike/moltenvk_metal_interop/` is
preserved for reference until SP4 ships, after which it can be deleted.
```

Fill in every `<...>` placeholder before saving — they're prompts, not the final content.

- [ ] **Step 3: Verify the spike note has no remaining placeholders**

Run:
```bash
grep -nE "<fill in|<yes|<no|<reason|<A /|<paste|<.../>|TBD|TODO" \
    /Users/mark/Documents/Projects/RetroNest-Project/docs/superpowers/notes/2026-05-23-moltenvk-metal-interop-spike.md
```

Expected: no output. If any matches, finish filling them in.

- [ ] **Step 4: Commit**

```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
git add docs/superpowers/notes/2026-05-23-moltenvk-metal-interop-spike.md
git commit -m "SP0: spike note — MoltenVK/Metal interop findings"
```

---

### Task 7: Close out and stage SP1

**Files:** None modified.

- [ ] **Step 1: Verify all deliverables exist**

Run:
```bash
cd /Users/mark/Documents/Projects/RetroNest-Project
ls -1 cpp/spike/moltenvk_metal_interop/{CMakeLists.txt,main.mm,README.md}
ls -1 docs/superpowers/notes/2026-05-23-moltenvk-metal-interop-spike.md
git log --oneline -5
```

Expected:
- Three spike files exist.
- Spike note exists.
- Recent commits include: scaffold, init bring-up, Path A, Path B, spike note.

- [ ] **Step 2: Summarize the result to the user**

Print to stdout (in your assistant response, not a script):
- The headline result from the spike note's Recommendation section.
- Which path SP4 will use.
- Whether SP1 (skeleton libretro core) can proceed without dependency on this decision (it can — SP4 is the first SP that touches Vulkan; SP1 only touches Metal).

- [ ] **Step 3: Nothing more to commit. Spike done.**

---

## Notes for the implementer

- The expected outcome of this spike is **knowledge**, not production code. Don't over-engineer the PoC.
- If Task 4's `vkExportMetalObjectsEXT` isn't exposed on this MoltenVK build, that's still a valid spike result — record it, complete Task 5, write the note with Path B as the recommendation. Don't go install a different MoltenVK to chase Path A; the spike documents what's available on the actual build target.
- The PoC links MoltenVK via the Vulkan loader (`libvulkan.dylib`), not directly. This matches how RetroNest will load Vulkan in SP4 — the standard loader is the production-realistic path.
- Image format choice: Path A uses `VK_FORMAT_R8G8B8A8_UNORM` and reads RGBA bytes back. Path B uses `VK_FORMAT_B8G8R8A8_UNORM` to match `MTLPixelFormatBGRA8Unorm` (the format IOSurface uses with `'BGRA'`). Different byte orders on readback — the assertions handle both. Don't accidentally unify them.
- If neither path works, the spike note's recommendation becomes "SP4 needs a third approach — investigate CVPixelBuffer-backed shared textures, or accept a per-frame `memcpy` from VkImage host-readback into MTLBuffer." This is a real escape hatch but should never be necessary on modern MoltenVK.
