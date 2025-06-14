#pragma once

#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <glm/glm.hpp>

struct ComputePipelineData
{
    std::string name;
    vk::Pipeline pipeline;
    vk::PipelineLayout pipelineLayout;
    void *pushData;
    uint32_t pushDataSize;
};

struct GradientColorPush
{
    glm::vec4 colorUp;
    glm::vec4 colorDown;
};

struct SkyPush
{
    glm::vec4 colorW;
};

std::vector<ComputePipelineData> init_background_compute_pipelines(
    const vk::Device &device, const vk::DescriptorSetLayout &descSetLayout);
