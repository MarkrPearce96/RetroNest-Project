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
