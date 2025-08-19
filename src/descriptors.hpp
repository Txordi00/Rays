#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif
#include "types.hpp"
#include <unordered_map>

// CAREFUL WITH THESE VALUES!
const std::unordered_map<vk::DescriptorType, uint32_t> BINDING_DICT
    = {{vk::DescriptorType::eUniformBuffer, 0},
       {vk::DescriptorType::eAccelerationStructureKHR, 0},
       {vk::DescriptorType::eStorageImage, 1}};

class DescHelper
{
public:
    DescHelper(const vk::Device &device,
               const vk::PhysicalDeviceProperties &physDevProp,
               const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties,
               const bool updateAfterBind);
    ~DescHelper() = default;
    void destroy();
    void add_descriptor_set(const vk::DescriptorPoolSize &poolSize, const uint32_t numSets);
    void create_descriptor_pool();
    void add_binding(const vk::DescriptorType &type, const vk::ShaderStageFlags &shaderStageFlags);
    vk::DescriptorSetLayout create_descriptor_set_layout();
    std::vector<vk::DescriptorSet> allocate_descriptor_sets(
        const vk::DescriptorSetLayout &descriptorSetLayout, const uint32_t frameOverlap);

private:
    const vk::Device &device;
    const vk::PhysicalDeviceProperties &physDevProp;
    const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties;
    const bool updateAfterBind;

    vk::DescriptorPool pool;
    std::vector<vk::DescriptorPoolSize> poolSizes;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

void update_descriptor_sets(const vk::Device &device,
                            const std::optional<std::vector<Buffer> > &uniformBuffers = std::nullopt,
                            const std::optional<vk::DescriptorSet> &uniformSet = std::nullopt,
                            const std::optional<vk::AccelerationStructureKHR> &tlas = std::nullopt,
                            const std::optional<vk::DescriptorSet> &tlasSet = std::nullopt,
                            const std::optional<vk::ImageView> &imageView = std::nullopt,
                            const std::optional<vk::DescriptorSet> &imageSet = std::nullopt);
