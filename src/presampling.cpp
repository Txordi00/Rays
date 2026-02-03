#include "presampling.hpp"
#include "utils.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtx/norm.hpp>
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
    create_images();
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

glm::vec4 Presampler::sample_microfacet_ggx_specular(const glm::vec2 &u, const float a)
{
    // Sample phi and theta in the local normal frame
    const float phi = glm::two_pi<float>() * u.x;
    const float a2 = a * a; // a in my case is already a = perceptualRoughness^2
    const float ctheta = (a2 > 1e-5) ? std::sqrt((1. - u.y) / (u.y * (a2 - 1.) + 1.)) : 1.;
    const float stheta = std::sqrt(1. - ctheta * ctheta);

    // Half vector in local frame
    const glm::vec3 hLocal = glm::vec3(stheta * std::cos(phi), stheta * std::sin(phi), ctheta);
    assert(glm::length2(hLocal) > 0.99 && glm::length2(hLocal) < 1.01);
    return glm::vec4(hLocal, 0.);
}

void Presampler::create_images()
{
    vk::SamplerCreateInfo samplerCreate{};
    samplerCreate.setMaxLod(vk::LodClampNone);
    samplerCreate.setMinLod(0.f);
    samplerCreate.setMagFilter(vk::Filter::eNearest);
    samplerCreate.setMinFilter(vk::Filter::eNearest);
    samplerCreate.setMipmapMode(vk::SamplerMipmapMode::eNearest);

    hemisphereImage = utils::create_image(device,
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
    hemisphereImage.sampler = device.createSampler(samplerCreate);

    ggxImage = utils::create_image(device,
                                   allocator,
                                   cmd,
                                   fence,
                                   queue,
                                   vk::Format::eR16G16B16A16Sfloat,
                                   vk::ImageUsageFlagBits::eSampled
                                       | vk::ImageUsageFlagBits::eTransferDst,
                                   vk::Extent3D{SAMPLING_DISCRETIZATION,
                                                SAMPLING_DISCRETIZATION,
                                                SAMPLING_DISCRETIZATION});
    ggxImage.sampler = device.createSampler(samplerCreate);
}

void Presampler::run()
{
    std::vector<std::uint64_t> hemisphereSamples(SAMPLING_DISCRETIZATION * SAMPLING_DISCRETIZATION);
    for (size_t i = 0; i < SAMPLING_DISCRETIZATION; i++) {
        const float v = static_cast<float>(i) / static_cast<float>(SAMPLING_DISCRETIZATION);
        for (size_t j = 0; j < SAMPLING_DISCRETIZATION; j++) {
            const float u = static_cast<float>(j) / static_cast<float>(SAMPLING_DISCRETIZATION);
            const glm::vec4 sample = cosine_sample_hemisphere(glm::vec2(u, v));
            hemisphereSamples[i * SAMPLING_DISCRETIZATION + j] = glm::packHalf4x16(sample);
        }
    }
    utils::copy_to_image(device,
                         allocator,
                         cmd,
                         fence,
                         queue,
                         hemisphereImage,
                         hemisphereImage.extent,
                         hemisphereSamples.data());

    std::vector<std::uint64_t> ggxSamples(SAMPLING_DISCRETIZATION * SAMPLING_DISCRETIZATION
                                          * SAMPLING_DISCRETIZATION);

    for (size_t k = 0; k < SAMPLING_DISCRETIZATION; k++) {
        const float a = static_cast<float>(k) / static_cast<float>(SAMPLING_DISCRETIZATION);
        for (size_t i = 0; i < SAMPLING_DISCRETIZATION; i++) {
            const float v = static_cast<float>(i) / static_cast<float>(SAMPLING_DISCRETIZATION);
            for (size_t j = 0; j < SAMPLING_DISCRETIZATION; j++) {
                const float u = static_cast<float>(j) / static_cast<float>(SAMPLING_DISCRETIZATION);
                const glm::vec4 sampleggx = sample_microfacet_ggx_specular(glm::vec2(u, v), a);
                ggxSamples[k * SAMPLING_DISCRETIZATION * SAMPLING_DISCRETIZATION
                           + i * SAMPLING_DISCRETIZATION + j]
                    = glm::packHalf4x16(sampleggx);
            }
        }
    }

    utils::copy_to_image(device,
                         allocator,
                         cmd,
                         fence,
                         queue,
                         ggxImage,
                         ggxImage.extent,
                         ggxSamples.data());
}

void Presampler::destroy()
{
    utils::destroy_image(device, allocator, hemisphereImage);
    utils::destroy_image(device, allocator, ggxImage);
}
