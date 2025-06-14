#include "raster_pipelines.hpp"
#include "types.hpp"

void GraphicsPipelineBuilder::clear()
{
    shaderStages = std::vector<vk::PipelineShaderStageCreateInfo>{};
    inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{};
    rasterizer = vk::PipelineRasterizationStateCreateInfo{};
    colorBlendAttachment = vk::PipelineColorBlendAttachmentState{};
    multisampling = vk::PipelineMultisampleStateCreateInfo{};
    pipelineLayout = vk::PipelineLayout{};
    depthStencil = vk::PipelineDepthStencilStateCreateInfo{};
    renderInfo = vk::PipelineRenderingCreateInfo{};
    colorAttachmentformat = vk::Format{};
}

vk::Pipeline GraphicsPipelineBuilder::buildPipeline(const vk::Device &device)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1);
    viewportState.setScissorCount(1);

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.setLogicOpEnable(vk::False);
    colorBlending.setLogicOp(vk::LogicOp::eCopy);
    colorBlending.setAttachments(colorBlendAttachment);

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    // connect the renderInfo to the pNext extension mechanism
    pipelineInfo.setPNext(&renderInfo);
    pipelineInfo.setStages(shaderStages);
    pipelineInfo.setPVertexInputState(&vertexInputInfo);
    pipelineInfo.setPInputAssemblyState(&inputAssembly);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&rasterizer);
    pipelineInfo.setPMultisampleState(&multisampling);
    pipelineInfo.setPColorBlendState(&colorBlending);
    pipelineInfo.setPDepthStencilState(&depthStencil);
    pipelineInfo.setLayout(pipelineLayout);

    std::vector<vk::DynamicState> states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.setDynamicStates(states);
    pipelineInfo.setPDynamicState(&dynamicInfo);

    vk::Result res;
    vk::Pipeline pipeline;
    try {
        std::tie(res, pipeline) = device.createGraphicsPipeline(nullptr, pipelineInfo);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    VK_CHECK_RES(res);

    return pipeline;
}
