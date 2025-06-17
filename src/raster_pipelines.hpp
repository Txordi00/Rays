#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <glm/glm.hpp>

struct TrianglePipelineData
{
    vk::PipelineLayout trianglePipelineLayout;
    vk::Pipeline trianglePipeline;
};

// push constants for our mesh object draws
struct TriangleMeshPush
{
    glm::mat4 worldMatrix;
    vk::DeviceAddress vertexBufferAddress;
};

TrianglePipelineData get_triangle_pipeline(const vk::Device &device,
                                           const vk::Format &colorImageFormat);

TrianglePipelineData get_triangle_mesh_pipeline(const vk::Device &device,
                                                const vk::Format &colorImageFormat);

class GraphicsPipelineBuilder
{
public:
    GraphicsPipelineBuilder() = default;
    ~GraphicsPipelineBuilder() = default;

    void clear();

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages{};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    vk::PipelineLayout pipelineLayout{};
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    vk::PipelineRenderingCreateInfo renderInfo{};
    vk::Format colorAttachmentformat{};

    void set_shaders(const vk::ShaderModule &vertexShader, const vk::ShaderModule &fragmentShader);
    void set_input_topology(const vk::PrimitiveTopology &topology);
    void set_polygon_mode(const vk::PolygonMode &mode);
    void set_cull_mode(const vk::CullModeFlags &cullMode, const vk::FrontFace &frontFace);
    void set_multisampling_none();
    void disable_blending();
    void set_color_attachment_format(const vk::Format &format);
    void set_depth_format(const vk::Format &format);
    void disable_depthtest();

    vk::Pipeline buildPipeline(const vk::Device &device);
};
