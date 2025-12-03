#include "presampling.hpp"
#include "utils.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_precision.hpp>
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
    // create_uniform_buffer();
    create_image();
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
    return nDotL / PI;
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

void Presampler::create_image()
{
    samplingImage = utils::create_image(device,
                                        allocator,
                                        cmd,
                                        fence,
                                        queue,
                                        vk::Format::eR16G16B16A16Sfloat,
                                        vk::ImageUsageFlagBits::eSampled
                                            | vk::ImageUsageFlagBits::eTransferDst,
                                        vk::Extent3D{SAMPLING_DISCRETIZATION,
                                                     SAMPLING_DISCRETIZATION,
                                                     1});

    vk::SamplerCreateInfo samplerCreate{};
    samplerCreate.setMaxLod(vk::LodClampNone);
    samplerCreate.setMinLod(0.f);
    samplerCreate.setMagFilter(vk::Filter::eNearest);
    samplerCreate.setMinFilter(vk::Filter::eNearest);
    samplerCreate.setMipmapMode(vk::SamplerMipmapMode::eNearest);

    samplingImage.sampler = device.createSampler(samplerCreate);
}

void Presampler::presample()
{
    std::array<std::uint64_t, SAMPLING_DISCRETIZATION * SAMPLING_DISCRETIZATION * 4>
        hemisphereSamples;
    for (size_t j = 0; j < SAMPLING_DISCRETIZATION; j++) {
        const float u = static_cast<float>(j) / static_cast<float>(SAMPLING_DISCRETIZATION);
        for (size_t i = 0; i < SAMPLING_DISCRETIZATION; i++) {
            const float v = static_cast<float>(i) / static_cast<float>(SAMPLING_DISCRETIZATION);
            const glm::vec4 sample = cosine_sample_hemisphere(glm::vec2(u, v));
            hemisphereSamples[j * SAMPLING_DISCRETIZATION + i] = glm::packHalf4x16(sample);
            // hemisphereSamples[j * SAMPLING_DISCRETIZATION + i + 1] = glm::packHalf1x16(sample.y);
            // hemisphereSamples[j * SAMPLING_DISCRETIZATION + i + 2] = glm::packHalf1x16(sample.z);
            // hemisphereSamples[j * SAMPLING_DISCRETIZATION + i + 3] = glm::packHalf1x16(sample.w);
        }
    }
    utils::copy_to_image(device,
                         allocator,
                         cmd,
                         fence,
                         queue,
                         samplingImage,
                         samplingImage.extent,
                         hemisphereSamples.data());

    // std::cout << "sizeof(glm::mediump_vec4): " << sizeof(glm::mediump_vec4) << std::endl;
    // std::cout << "Expected size: " << (4 * 2) << " bytes" << std::endl;
    // std::cout << "Total data size: " << sizeof(hemisphereSamples) << std::endl;
    // std::cout << "Expected total: " << (SAMPLING_DISCRETIZATION * SAMPLING_DISCRETIZATION * 4 * 2)
    //           << std::endl;

    // utils::copy_to_device_buffer(uniformBuffer, device, allocator, cmd, queue, fence, &uboData);
}

void Presampler::destroy()
{
    utils::destroy_image(device, allocator, samplingImage);
    // utils::destroy_buffer(allocator, uniformBuffer);
}

// void Presampler::create_uniform_buffer()
// {
//     vk::DeviceSize uboSize = sizeof(UniformData);
//     uniformBuffer = utils::create_buffer(device,
//                                          allocator,
//                                          uboSize,
//                                          vk::BufferUsageFlagBits::eUniformBuffer
//                                              | vk::BufferUsageFlagBits::eTransferDst,
//                                          VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
// }
