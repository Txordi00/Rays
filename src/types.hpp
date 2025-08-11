#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <iostream>
#include <vk_mem_alloc.h>

#define VK_CHECK_RES(x) \
    { \
        vk::Result res = static_cast<vk::Result>(x); \
        if (res != vk::Result::eSuccess) { \
            std::cerr << "\033[1;33m" << "NON SUCCESSFUL vk::Result: " << vk::to_string(res) \
                      << "\033[0m" << std::endl; \
        } \
    }

// To be used inside the catch part of a try and catch statement
#define VK_CHECK_EXC(exception) \
    { \
        std::string err_str = std::string("\033[1;33m + Vulkan exception: ") + exception.what() \
                              + std::string("\033[0m\n"); \
        throw std::runtime_error(err_str); \
    }

const unsigned int W = 1000;
const unsigned int H = 1000;
#define PROJNAME "LRT"
const unsigned int API_VERSION[3] = {1, 4, 0};

const vk::PresentModeKHR PRESENT_MODE = vk::PresentModeKHR::eFifoRelaxed;
const unsigned int MINIMUM_FRAME_OVERLAP = 2;
const uint64_t FENCE_TIMEOUT = 1000000000;

#define GRADIENT_COMP_SHADER_FP "shaders/gradient.comp.spv"
#define GRADIENT_COLOR_COMP_SHADER_FP "shaders/gradient_color.comp.spv"
#define SKY_SHADER_FP "shaders/sky.comp.spv"
#define TRIANGLE_VERT_SHADER "shaders/triangle.vert.spv"
#define TRIANGLE_FRAG_SHADER "shaders/triangle.frag.spv"
#define SIMPLE_MESH_VERT_SHADER "shaders/triangle_mesh.vert.spv"

struct DescriptorSetData
{
    vk::DescriptorSetLayout layout;
    vk::DescriptorType type;
    uint32_t descriptorCount;
};

struct FrameData
{
    vk::CommandPool commandPool;
    vk::CommandBuffer mainCommandBuffer;
    vk::Semaphore /*swapchainSemaphore,*/ renderSemaphore;
    vk::Fence renderFence;
};

struct ImageData
{
    vk::Image image;
    vk::ImageView imageView;
    VmaAllocation allocation;
    vk::Extent3D extent;
    vk::Format format;
};

struct Buffer
{
    vk::Buffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
    uint32_t bufferId;
};

struct Vertex
{
    glm::vec3 position;
    float uvX;
    glm::vec3 normal;
    float uvY;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct MeshBuffer
{
    Buffer indexBuffer;
    Buffer vertexBuffer;
    vk::DeviceAddress vertexBufferAddress;
    vk::DeviceAddress indexBufferAddress;
};

// push constants for our mesh object draws
struct MeshPush
{
    // glm::mat4 worldMatrix;
    uint32_t objId;
    vk::DeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct UniformData
{
    glm::mat4 worldMatrix;
};

struct ComputePipelineData
{
    std::string name;
    vk::Pipeline pipeline;
    vk::PipelineLayout pipelineLayout;
    void *pushData;
    uint32_t pushDataSize;
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

