#pragma once
#include "types.hpp"

#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

// CHECK THESE VALUES
const uint32_t BINDING_UNIFORM = 0;
const uint32_t BINDING_TLAS = 0;
const uint32_t BINDING_OUT_IMG = 1;

class DescHelper
{
public:
    DescHelper(const vk::Device &device,
               const vk::PhysicalDeviceProperties &physDevProp,
               const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties);
    ~DescHelper() = default;
    void destroy();
    void add_descriptor_set(const vk::DescriptorPoolSize &poolSize,
                            const uint32_t numSets,
                            const bool updateAfterBind);
    void create_descriptor_pools();
    std::pair<vk::DescriptorSetLayout, vk::DescriptorSetLayout> create_descriptor_set_layouts();
    std::pair<std::vector<vk::DescriptorSet>, std::vector<vk::DescriptorSet>> allocate_descriptor_sets(
        const std::pair<vk::DescriptorSetLayout, vk::DescriptorSetLayout> &descriptorSetLayouts,
        const uint32_t frameOverlap);
    void update_descriptor_sets(const std::vector<Buffer> &uniformBuffers,
                                const vk::DescriptorSet &uniformSet,
                                const vk::AccelerationStructureKHR &tlas,
                                const vk::DescriptorSet &tlasSet,
                                const vk::ImageView &imageView,
                                const vk::DescriptorSet &imageSet);

private:
    const vk::Device &device;
    const vk::PhysicalDeviceProperties &physDevProp;
    const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties;

    std::vector<vk::DescriptorPoolSize> poolSizesUAB;
    std::vector<vk::DescriptorPoolSize> poolSizesNonUAB;
    vk::DescriptorPool poolUAB;
    vk::DescriptorPool poolNonUAB;
    uint32_t maxUniformDescriptors{0};
    uint32_t maxStorageImageDescriptors{0};
    uint32_t maxASDescriptors{0};
};
