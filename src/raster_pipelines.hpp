#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

class GraphicsPipelineBuilder
{
public:
    GraphicsPipelineBuilder() = default;

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

    vk::Pipeline buildPipeline(const vk::Device &device);
};
