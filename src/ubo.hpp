#pragma once
#include "types.hpp"

class Ubo
{
public:
    Ubo(const vk::Device &device);
    ~Ubo();
    void create_descriptor_pool();
    void create_descriptor_set_layout();
    vk::DescriptorSet allocate_descriptor_set();
    void create_pipeline_layout(const vk::PushConstantRange &pushRange);
    Buffer create_buffer(const uint32_t bufferId);
    void update_buffer(const Buffer &buffer, const void *data);
    void update_descriptor_sets(const std::vector<Buffer> &buffers,
                                const std::vector<VkDescriptorSet> &descriptorSets);

private:
    const vk::Device &device;
};
