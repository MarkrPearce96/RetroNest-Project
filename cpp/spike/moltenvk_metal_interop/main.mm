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
    // ---- VkInstance ----
    // VK_EXT_metal_objects is a device extension only; it does not belong here.
    const char* instanceExts[] = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
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
    // NOTE: plane must be VK_IMAGE_ASPECT_COLOR_BIT for non-planar (single-plane) images.
    // VK_IMAGE_ASPECT_PLANE_0_BIT is only valid for multi-planar (YCbCr) formats.
    VkExportMetalTextureInfoEXT tinfo = {VK_STRUCTURE_TYPE_EXPORT_METAL_TEXTURE_INFO_EXT};
    tinfo.image = image;
    tinfo.plane = VK_IMAGE_ASPECT_COLOR_BIT;
    VkExportMetalObjectsInfoEXT info = {VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT};
    info.pNext = &tinfo;
    pfnExport(device, &info);
    id<MTLTexture> mtlTex = tinfo.mtlTexture;
    if (!mtlTex) {
        fprintf(stderr, "[Path A] export returned nil MTLTexture\n");
        vkFreeMemory(device, mem, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }

    // 5) Read back via Metal and verify bytes.
    // getBytes is only valid for CPU-accessible textures (Shared or Managed storage).
    // VK_IMAGE_TILING_OPTIMAL + device-local memory maps to MTLStorageModePrivate on
    // Apple Silicon; calling getBytes on it crashes. Check storageMode first.
    if (mtlTex.storageMode != MTLStorageModeShared && mtlTex.storageMode != MTLStorageModeManaged) {
        fprintf(stderr, "[Path A] MTLTexture storageMode=%lu is GPU-private; "
                "getBytes not supported. Export succeeded but CPU readback requires "
                "a blit to a shared texture (out of scope for this PoC).\n",
                (unsigned long)mtlTex.storageMode);
        vkFreeMemory(device, mem, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkDestroyCommandPool(device, pool, nullptr);
        return false;
    }
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

// Stub implementation — Task 5 fills this in.
static bool runPathB(VkInstance, VkPhysicalDevice, VkDevice) {
    fprintf(stderr, "[Path B] not yet implemented\n");
    return false;
}
