#ifndef USE_CXX20_MODULES
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#else
import vulkan_hpp;
#include <vulkan/vulkan_hpp_macros.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif
// #define VMA_IMPLEMENTATION
#include "engine.hpp"
#include <memory>

int main(int argc, char *argv[])
{
    // Load the basic functionality of the dynamic dispatcher
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
#ifndef USE_CXX20_MODULES
    vk::detail::DynamicLoader dl;
    VULKAN_HPP_DEFAULT_DISPATCHER.init(dl);
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>(
        "vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(getInstanceProcAddr);
#endif

    std::unique_ptr<Engine> engine = std::make_unique<Engine>();

    engine->run();

    return 0;
}
