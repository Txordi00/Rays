#include "pipelines_compute.hpp"
#include "types.hpp"
#include "utils.hpp"

std::vector<ComputePipelineData> init_background_compute_pipelines(
    const vk::Device &device, const vk::DescriptorSetLayout &descSetLayout)
{
    std::vector<ComputePipelineData> pipelines(2);

    pipelines[0].name = "gradientcolor";

    vk::PushConstantRange pushColors{};
    pushColors.setOffset(0);
    pushColors.setSize(sizeof(GradientColorPush));
    pushColors.setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::PipelineLayoutCreateInfo gradColorComputeLayoutCreateInfo{};
    gradColorComputeLayoutCreateInfo.setSetLayouts(descSetLayout);
    gradColorComputeLayoutCreateInfo.setPushConstantRanges(pushColors);
    pipelines[0].pipelineLayout = device.createPipelineLayout(gradColorComputeLayoutCreateInfo);

    vk::ShaderModule gradColorComputeShader = utils::load_shader(device,
                                                                 GRADIENT_COLOR_COMP_SHADER_FP);

    vk::PipelineShaderStageCreateInfo gradColorComputeStageCreate{};
    gradColorComputeStageCreate.setModule(gradColorComputeShader);
    gradColorComputeStageCreate.setPName("main");
    gradColorComputeStageCreate.setStage(vk::ShaderStageFlagBits::eCompute);

    vk::ComputePipelineCreateInfo gradColorComputePipelineCreate{};
    gradColorComputePipelineCreate.setLayout(pipelines[0].pipelineLayout);
    gradColorComputePipelineCreate.setStage(gradColorComputeStageCreate);

    vk::Result res;
    vk::Pipeline val;
    try {
        std::tie(res, val) = device.createComputePipeline(nullptr, gradColorComputePipelineCreate);
        pipelines[0].pipeline = val;
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    VK_CHECK_RES(res);

    static GradientColorPush defaultGradColorPush{};
    defaultGradColorPush.colorUp = glm::vec4{1., 0., 0., 1.};
    defaultGradColorPush.colorDown = glm::vec4{0., 1., 0., 1.};
    pipelines[0].pushData = (void *) &defaultGradColorPush;
    pipelines[0].pushDataSize = sizeof(GradientColorPush);

    device.destroyShaderModule(gradColorComputeShader);

    pipelines[1].name = "sky";

    vk::PushConstantRange pushSky{};
    pushSky.setOffset(0);
    pushSky.setSize(sizeof(SkyPush));
    pushSky.setStageFlags(vk::ShaderStageFlagBits::eCompute);

    vk::PipelineLayoutCreateInfo skyComputeLayoutCreateInfo{};
    skyComputeLayoutCreateInfo.setSetLayouts(descSetLayout);
    skyComputeLayoutCreateInfo.setPushConstantRanges(pushSky);
    pipelines[1].pipelineLayout = device.createPipelineLayout(skyComputeLayoutCreateInfo);

    vk::ShaderModule skyComputeShader = utils::load_shader(device, SKY_SHADER_FP);

    vk::PipelineShaderStageCreateInfo skyComputeStageCreate{};
    skyComputeStageCreate.setModule(skyComputeShader);
    skyComputeStageCreate.setPName("main");
    skyComputeStageCreate.setStage(vk::ShaderStageFlagBits::eCompute);

    vk::ComputePipelineCreateInfo skyComputePipelineCreate{};
    skyComputePipelineCreate.setLayout(pipelines[1].pipelineLayout);
    skyComputePipelineCreate.setStage(skyComputeStageCreate);

    try {
        std::tie(res, val) = device.createComputePipeline(nullptr, skyComputePipelineCreate);
        pipelines[1].pipeline = val;
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    VK_CHECK_RES(res);

    static SkyPush skyPush{};
    skyPush.colorW = glm::vec4{0.1, 0.2, 0.4, 0.97};
    pipelines[1].pushData = (void *) &skyPush;
    pipelines[1].pushDataSize = sizeof(SkyPush);

    device.destroyShaderModule(skyComputeShader);

    return pipelines;
}
