#include "presampling.hpp"
#include "utils.hpp"
#include <glm/gtc/constants.hpp>
#include <print>

#define PI glm::pi<float>()

Presampler::Presampler(const vk::Device &device,
                       const VmaAllocator &allocator,
                       const vk::CommandBuffer &cmd,
                       const vk::Queue &queue,
                       const vk::Fence &fence)
    : device{device}
    , allocator{allocator}
    , cmd{cmd}
    , queue{queue}
    , fence{fence}
{
    create_uniform_buffer();
}

glm::vec2 Presampler::concentric_sample_disk(const glm::vec2 &u)
{
    glm::vec2 uOffset = 2.f * u - glm::vec2(1.f);
    if (glm::abs(uOffset.x) < 0.001f && glm::abs(uOffset.y) < 0.001f) {
        return glm::vec2(0.f);
    }
    float theta, r;
    if (glm::abs(uOffset.x) > glm::abs(uOffset.y)) {
        r = uOffset.x;
        theta = PI / 4.f * (uOffset.y / uOffset.x);
    } else {
        r = uOffset.y;
        theta = PI / 2.f - PI / 4.f * (uOffset.x / uOffset.y);
    }
    return r * glm::vec2(glm::cos(theta), glm::sin(theta));
}

float Presampler::pdf_cosine_sample_hemisphere(const float nDotL)
{
    return nDotL * 1.f / PI;
}

glm::vec4 Presampler::cosine_sample_hemisphere(const glm::vec2 &u)
{
    const glm::vec2 d = concentric_sample_disk(u);
    const float d2 = glm::dot(d, d);
    const float z = glm::sqrt(glm::max(0.f, 1.f - d2));
    const glm::vec3 sample = normalize(glm::vec3(d.x, d.y, z));

    const float nDotL = sample.z;
    const float pdf = pdf_cosine_sample_hemisphere(nDotL);

    glm::vec4 sampleInNormalFrame = glm::vec4(sample, pdf);
    return sampleInNormalFrame;
}

void Presampler::create_uniform_buffer()
{
    vk::DeviceSize uboSize = sizeof(UniformData);
    uniformBuffer = utils::create_buffer(device,
                                         allocator,
                                         uboSize,
                                         vk::BufferUsageFlagBits::eUniformBuffer
                                             | vk::BufferUsageFlagBits::eTransferDst,
                                         VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
}

void Presampler::presample()
{
    UniformData uboData{};
    for (size_t j = 0; j < SAMPLING_DISCRETIZATION; j++) {
        const float u = static_cast<float>(j) / static_cast<float>(SAMPLING_DISCRETIZATION);
        for (size_t i = 0; i < SAMPLING_DISCRETIZATION; i++) {
            const float v = static_cast<float>(i) / static_cast<float>(SAMPLING_DISCRETIZATION);
            uboData.hemisphereSamples[j * SAMPLING_DISCRETIZATION + i] = cosine_sample_hemisphere(
                glm::vec2(u, v));
        }
    }
    utils::copy_to_device_buffer(uniformBuffer, device, allocator, cmd, queue, fence, &uboData);
}
