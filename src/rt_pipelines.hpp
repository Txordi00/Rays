#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan;
#endif

#include "types.hpp"

SimplePipelineData get_simple_rt_pipeline(const vk::Device &device);

class RtPipelineBuilder
{
public:
    enum StageIndices { eRaygen, eMiss, eShadow, eClosestHit, eShaderStageCount };

    RtPipelineBuilder(const vk::Device &device)
        : device{device}
    {}
    ~RtPipelineBuilder() = default;

    void create_shader_stages();

    void create_shader_groups();

    vk::PipelineLayout buildPipelineLayout(
        const std::vector<vk::DescriptorSetLayout> &descSetLayouts);

    vk::Pipeline buildPipeline(const vk::PipelineLayout &pipelineLayout,
                               const SpecializationConstantsClosestHit &constantsCH = {},
                               const SpecializationConstantsMiss &constantsMiss = {});

    void destroy();

private:
    const vk::Device &device;
    std::array<vk::PipelineShaderStageCreateInfo, eShaderStageCount> shaderStages;
    std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
    std::vector<vk::Pipeline> pipelineQueue;
};
