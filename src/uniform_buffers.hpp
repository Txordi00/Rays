#pragma once
#include "types.hpp"

class Ubo
{
public:
    Ubo(const vk::Device &device)
        : device{device} {};
    ~Ubo() = default;
    void destroy();
    void create_descriptor_pool(const uint32_t maxDescriptorCount, const uint32_t maxSets);
    vk::DescriptorSetLayout create_descriptor_set_layout(const vk::ShaderStageFlags &shaderStages);
    std::vector<vk::DescriptorSet> allocate_descriptor_sets(
        const vk::DescriptorSetLayout &descriptorSetLayout, const uint32_t descriptorSetCount);
    void update_descriptor_sets(const std::vector<Buffer> &buffers,
                                const vk::DescriptorSet &descriptorSet);

private:
    const vk::Device &device;
    vk::DescriptorPool pool;
    uint32_t maxDescriptors;
};
