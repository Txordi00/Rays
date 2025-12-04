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

    void run();

    void destroy();

    ImageData hemisphereImage;
    ImageData ggxImage;

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
    const vk::CommandBuffer &cmd;
    const vk::Queue &queue;
    const vk::Fence &fence;


    glm::vec2 concentric_sample_disk(const glm::vec2 &u);

    float pdf_cosine_sample_hemisphere(const float nDotL);

    glm::vec4 cosine_sample_hemisphere(const glm::vec2 &u);

    glm::vec4 sample_microfacet_ggx_specular(const glm::vec2 &u, const float a);

    void create_images();
};
