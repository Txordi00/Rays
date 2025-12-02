#pragma once

#include "types.hpp"
class Presampler
{
    struct UniformData
    {
        glm::vec4 hemisphereSamples[SAMPLING_DISCRETIZATION * SAMPLING_DISCRETIZATION];
    };

public:
    Presampler(const vk::Device &device,
               const VmaAllocator &allocator,
               const vk::CommandBuffer &cmd,
               const vk::Queue &queue,
               const vk::Fence &fence);
    ~Presampler() = default;

    void presample();

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
    const vk::CommandBuffer &cmd;
    const vk::Queue &queue;
    const vk::Fence &fence;

    Buffer uniformBuffer;

    glm::vec2 concentric_sample_disk(const glm::vec2 &u);

    float pdf_cosine_sample_hemisphere(const float nDotL);

    glm::vec4 cosine_sample_hemisphere(const glm::vec2 &u);

    void create_uniform_buffer();
};
