#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif
#include "types.hpp"

class SbtHelper
{
public:
    SbtHelper(const vk::Device &device,
              const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR &rtProperties)
        : device{device}
        , rtProperties{rtProperties}
    {}
    ~SbtHelper() = default;

    Buffer create_shader_binding_table();

private:
    const vk::Device &device;
    const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR &rtProperties;
};
