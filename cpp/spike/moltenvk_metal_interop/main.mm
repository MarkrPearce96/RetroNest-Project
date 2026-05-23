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

// Stub implementations — Tasks 4 and 5 fill these in.
static bool runPathA(VkInstance, VkPhysicalDevice, VkDevice) {
    fprintf(stderr, "[Path A] not yet implemented\n");
    return false;
}
static bool runPathB(VkInstance, VkPhysicalDevice, VkDevice) {
    fprintf(stderr, "[Path B] not yet implemented\n");
    return false;
}
