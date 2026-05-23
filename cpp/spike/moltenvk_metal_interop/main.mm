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
