#pragma once

#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "types.hpp"
#include <glm/glm.hpp>

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
