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

struct DescriptorSetData
{
    vk::DescriptorSetLayout layout;
    vk::DescriptorType type;
    uint32_t descriptorCount;
};

class DescriptorPool
{
public:
    DescriptorPool(const vk::Device &device,
                   const std::vector<DescriptorSetData> &descriptorSets,
                   const uint32_t maxSets);
    ~DescriptorPool() = default;
    vk::DescriptorPool create(const vk::DescriptorPoolCreateFlags &descriptorPoolCreateFlags);
    std::vector<vk::DescriptorSet> allocate_descriptors(const std::vector<unsigned int> &indexes);
    void reset();
    void destroyPool();

    vk::DescriptorPool getPool() const { return pool; }

private:
    vk::Device device;
    std::vector<DescriptorSetData> descriptorSets;
    uint32_t maxSets;
    vk::DescriptorPool pool;
};
