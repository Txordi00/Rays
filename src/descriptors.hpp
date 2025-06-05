#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

class DescriptorSetLayout
{
public:
    DescriptorSetLayout(const vk::Device &device);
    ~DescriptorSetLayout() = default;
    void add_binding(vk::DescriptorSetLayoutBinding binding);
    vk::DescriptorSetLayout get_descriptor_set(
        vk::DescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags);
    void reset();

private:
    vk::Device device;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    vk::DescriptorSetLayout layout;
};

struct DescriptorData
{
    vk::DescriptorSetLayout layout;
    vk::DescriptorType type;
    uint32_t descriptorCount;
};

class DescriptorPool
{
public:
    DescriptorPool(const vk::Device &device,
                   const std::vector<DescriptorData> &descriptorSets,
                   const uint32_t maxSets);
    ~DescriptorPool();
    vk::DescriptorPool create(const vk::DescriptorPoolCreateFlags &descriptorPoolCreateFlags);
    void reset();

private:
    vk::Device device;
    std::vector<DescriptorData> descriptors;
    uint32_t maxSets;
    vk::DescriptorPool pool;
};
