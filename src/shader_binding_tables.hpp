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
              const VmaAllocator &allocator,
              const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR &rtProperties)
        : device{device}
        , allocator{allocator}
        , rtProperties{rtProperties}
    {}
    ~SbtHelper() = default;

    Buffer create_shader_binding_table(const vk::Pipeline &rtPipeline);

    vk::StridedDeviceAddressRegionKHR rgenRegion;
    vk::StridedDeviceAddressRegionKHR missRegion;
    vk::StridedDeviceAddressRegionKHR hitRegion;

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
    const vk::PhysicalDeviceRayTracingPipelinePropertiesKHR &rtProperties;
};
