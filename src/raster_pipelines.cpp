#include "raster_pipelines.hpp"
#include "types.hpp"
#include "utils.hpp"

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

void GraphicsPipelineBuilder::set_shaders(const vk::ShaderModule &vertexShader,
                                          const vk::ShaderModule &fragmentShader)
{
    shaderStages.clear();
    shaderStages.resize(2);

    vk::PipelineShaderStageCreateInfo vertexInfo{};
    vertexInfo.setModule(vertexShader);
    vertexInfo.setPName("main");
    vertexInfo.setStage(vk::ShaderStageFlagBits::eVertex);

    vk::PipelineShaderStageCreateInfo fragmentInfo{};
    fragmentInfo.setModule(fragmentShader);
    fragmentInfo.setPName("main");
    fragmentInfo.setStage(vk::ShaderStageFlagBits::eFragment);

    shaderStages[0] = vertexInfo;
    shaderStages[1] = fragmentInfo;
}
void GraphicsPipelineBuilder::set_input_topology(const vk::PrimitiveTopology &topology)
{
    inputAssembly.setTopology(topology);
    inputAssembly.setPrimitiveRestartEnable(vk::False);
}

void GraphicsPipelineBuilder::set_polygon_mode(const vk::PolygonMode &mode)
{
    rasterizer.setPolygonMode(mode);
    rasterizer.setLineWidth(1.f);
}

void GraphicsPipelineBuilder::set_cull_mode(const vk::CullModeFlags &cullMode,
                                            const vk::FrontFace &frontFace)
{
    rasterizer.setCullMode(cullMode);
    rasterizer.setFrontFace(frontFace);
}

void GraphicsPipelineBuilder::set_multisampling_none()
{
    multisampling.setSampleShadingEnable(vk::False);
    // multisampling defaulted to no multisampling (1 sample per pixel)
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    multisampling.setMinSampleShading(1.0f);
    multisampling.setPSampleMask(nullptr);
    // no alpha to coverage either
    multisampling.setAlphaToCoverageEnable(vk::False);
    multisampling.setAlphaToOneEnable(vk::False);
}

void GraphicsPipelineBuilder::disable_blending()
{
    // default write mask
    colorBlendAttachment.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    // no blending
    colorBlendAttachment.setBlendEnable(vk::False);
}

void GraphicsPipelineBuilder::set_color_attachment_format(const vk::Format &format)
{
    colorAttachmentformat = format;
    // connect the format to the renderInfo  structure
    renderInfo.setColorAttachmentFormats(colorAttachmentformat);
}

void GraphicsPipelineBuilder::set_depth_format(const vk::Format &format)
{
    renderInfo.setDepthAttachmentFormat(format);
}

void GraphicsPipelineBuilder::disable_depthtest()
{
    depthStencil.setDepthTestEnable(vk::False);
    depthStencil.setDepthWriteEnable(vk::False);
    depthStencil.setDepthCompareOp(vk::CompareOp::eNever);
    depthStencil.setDepthBoundsTestEnable(vk::False);
    depthStencil.setStencilTestEnable(vk::False);
    depthStencil.setFront(vk::StencilOpState{});
    depthStencil.setBack(vk::StencilOpState{});
    depthStencil.setMinDepthBounds(0.f);
    depthStencil.setMaxDepthBounds(1.f);
}

void GraphicsPipelineBuilder::enable_depthtest()
{
    depthStencil.setDepthTestEnable(vk::True);
    depthStencil.setDepthWriteEnable(vk::True);
    depthStencil.setDepthCompareOp(vk::CompareOp::eLess);
    depthStencil.setDepthBoundsTestEnable(vk::False);
    depthStencil.setStencilTestEnable(vk::False);
    // depthStencil.setFront(vk::StencilOpState{});
    // depthStencil.setBack(vk::StencilOpState{});
    depthStencil.setMinDepthBounds(0.f);
    depthStencil.setMaxDepthBounds(1.f);
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

SimplePipelineData get_triangle_pipeline(const vk::Device &device,
                                         const vk::Format &colorImageFormat)
{
    SimplePipelineData trianglePipelineData;

    vk::ShaderModule vertexShader = utils::load_shader(device, TRIANGLE_VERT_SHADER);
    vk::ShaderModule fragmentShader = utils::load_shader(device, TRIANGLE_FRAG_SHADER);

    //build the pipeline layout that controls the inputs/outputs of the shader
    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayouts(nullptr);

    try {
        trianglePipelineData.pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }

    GraphicsPipelineBuilder pipelineBuilder{};
    //use the triangle layout we created
    pipelineBuilder.pipelineLayout = trianglePipelineData.pipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(vertexShader, fragmentShader);
    //it will draw triangles
    pipelineBuilder.set_input_topology(vk::PrimitiveTopology::eTriangleList);
    //filled triangles
    pipelineBuilder.set_polygon_mode(vk::PolygonMode::eFill);
    //no backface culling
    pipelineBuilder.set_cull_mode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise);
    //no multisampling
    pipelineBuilder.set_multisampling_none();
    //no blending
    pipelineBuilder.disable_blending();
    //no depth testing
    pipelineBuilder.disable_depthtest();

    //connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(colorImageFormat);
    pipelineBuilder.set_depth_format(vk::Format::eUndefined);

    try {
        trianglePipelineData.pipeline = pipelineBuilder.buildPipeline(device);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    device.destroyShaderModule(vertexShader);
    device.destroyShaderModule(fragmentShader);

    return trianglePipelineData;
}

SimplePipelineData get_simple_mesh_pipeline(
    const vk::Device &device,
    const vk::Format &colorImageFormat,
    const vk::Format &depthImageFormat,
    const std::vector<vk::DescriptorSetLayout> &descriptorLayouts)
{
    SimplePipelineData trianglePipelineData;

    vk::ShaderModule vertexShader = utils::load_shader(device, SIMPLE_MESH_VERT_SHADER);
    vk::ShaderModule fragmentShader = utils::load_shader(device, TRIANGLE_FRAG_SHADER);

    //build the pipeline layout that controls the inputs/outputs of the shader
    //we are not using descriptor sets or other systems yet, so no need to use anything other than empty default
    vk::PushConstantRange pushInfo{};
    pushInfo.setOffset(0);
    pushInfo.setStageFlags(vk::ShaderStageFlagBits::eVertex);
    pushInfo.setSize(sizeof(MeshPush));
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayouts(descriptorLayouts);
    pipelineLayoutInfo.setPushConstantRanges(pushInfo);

    try {
        trianglePipelineData.pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }

    GraphicsPipelineBuilder pipelineBuilder{};
    //use the triangle layout we created
    pipelineBuilder.pipelineLayout = trianglePipelineData.pipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(vertexShader, fragmentShader);
    //it will draw triangles
    pipelineBuilder.set_input_topology(vk::PrimitiveTopology::eTriangleList);
    //filled triangles
    pipelineBuilder.set_polygon_mode(vk::PolygonMode::eFill);
    //set depth testing
    pipelineBuilder.enable_depthtest();
    // //no depth testing
    // pipelineBuilder.disable_depthtest();
    //no backface culling
    pipelineBuilder.set_cull_mode(vk::CullModeFlagBits::eNone, vk::FrontFace::eClockwise);
    //no multisampling
    pipelineBuilder.set_multisampling_none();
    //no blending
    pipelineBuilder.disable_blending();

    //connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(colorImageFormat);
    pipelineBuilder.set_depth_format(depthImageFormat);

    try {
        trianglePipelineData.pipeline = pipelineBuilder.buildPipeline(device);
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    device.destroyShaderModule(vertexShader);
    device.destroyShaderModule(fragmentShader);

    return trianglePipelineData;
}
