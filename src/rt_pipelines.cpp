#include "rt_pipelines.hpp"
#include "utils.hpp"

void RtPipelineBuilder::create_shader_stages()
{
    // All stages
    vk::PipelineShaderStageCreateInfo stage{};
    stage.setPName("main"); // All the same entry point
    // Raygen
    stage.setModule(utils::load_shader(device, SIMPLE_RGEN_SHADER));
    stage.setStage(vk::ShaderStageFlagBits::eRaygenKHR);
    shaderStages[eRaygen] = stage;
    // Miss
    stage.setModule(utils::load_shader(device, SIMPLE_RMISS_SHADER));
    stage.setStage(vk::ShaderStageFlagBits::eMissKHR);
    shaderStages[eMiss] = stage;
    // Hit Group - Closest Hit
    stage.setModule(utils::load_shader(device, SIMPLE_RCHIT_SHADER));
    stage.setStage(vk::ShaderStageFlagBits::eClosestHitKHR);
    shaderStages[eClosestHit] = stage;
}

void RtPipelineBuilder::create_shader_groups()
{
    // Shader groups
    vk::RayTracingShaderGroupCreateInfoKHR group{};
    group.setAnyHitShader(vk::ShaderUnusedKHR);
    group.setAnyHitShader(vk::ShaderUnusedKHR);
    group.setGeneralShader(vk::ShaderUnusedKHR);
    group.setIntersectionShader(vk::ShaderUnusedKHR);

    // Raygen
    group.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    group.setGeneralShader(eRaygen);
    shaderGroups.push_back(group);

    // Miss
    group.setType(vk::RayTracingShaderGroupTypeKHR::eGeneral);
    group.setGeneralShader(eMiss);
    shaderGroups.push_back(group);

    // closest hit shader
    group.setType(vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup);
    group.setGeneralShader(vk::ShaderUnusedKHR);
    group.setClosestHitShader(eClosestHit);
    shaderGroups.push_back(group);
}

// The first descriptor should be the one with the AS and the output image!
vk::PipelineLayout RtPipelineBuilder::buildPipelineLayout(
    const std::vector<vk::DescriptorSetLayout> &descSetLayouts)
{
    // Push constant: we want to be able to update constants used by the shaders
    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.setOffset(0);
    pushConstantRange.setSize(sizeof(RayPush));
    pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR
                                    | vk::ShaderStageFlagBits::eClosestHitKHR
                                    | vk::ShaderStageFlagBits::eMissKHR);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.setPushConstantRanges(pushConstantRange);
    pipelineLayoutCreateInfo.setSetLayouts(descSetLayouts);

    return device.createPipelineLayout(pipelineLayoutCreateInfo);
}

vk::Pipeline RtPipelineBuilder::buildPipeline(const vk::PipelineLayout &pipelineLayout)
{
    // Assemble the shader stages and recursion depth info into the ray tracing pipeline
    vk::RayTracingPipelineCreateInfoKHR rtPipelineInfo{};
    rtPipelineInfo.setStages(shaderStages); // Stages are shaders
    // In this case, shaderGroups.size() == 3: we have one raygen group,
    // one miss shader group, and one hit group.
    rtPipelineInfo.setGroups(shaderGroups);

    rtPipelineInfo.setMaxPipelineRayRecursionDepth(MAX_RT_RECURSION); // Ray depth

    rtPipelineInfo.setLayout(pipelineLayout);

    auto [res, val] = device.createRayTracingPipelinesKHR(nullptr, nullptr, rtPipelineInfo);
    VK_CHECK_RES(res);

    for (const auto &s : shaderStages)
        device.destroyShaderModule(s.module);

    return val[0];
}
