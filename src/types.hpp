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
const float F = 1.f; // focal length
const float FOV = glm::radians(70.f);
#define PROJNAME "LRT"
const unsigned int API_VERSION[3] = {1, 4, 0};

const vk::PresentModeKHR PRESENT_MODE = vk::PresentModeKHR::eFifoRelaxed;
const unsigned int MINIMUM_FRAME_OVERLAP = 2;
const uint64_t FENCE_TIMEOUT = 1000000000;
const uint32_t MAX_RT_RECURSION = 1;

#define SIMPLE_MESH_FRAG_SHADER "shaders/simple_mesh.frag.spv"
#define SIMPLE_MESH_VERT_SHADER "shaders/simple_mesh.vert.spv"
#define SIMPLE_RCHIT_SHADER "shaders/raytrace.rchit.spv"
#define SIMPLE_RGEN_SHADER "shaders/raytrace.rgen.spv"
#define SIMPLE_RMISS_SHADER "shaders/raytrace.rmiss.spv"

struct DescriptorSetData
{
    vk::DescriptorSetLayout layout;
    vk::DescriptorType type;
    uint32_t descriptorCount;
};

struct SimplePipelineData
{
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;
};

struct FrameData
{
    vk::CommandPool commandPool;
    vk::CommandBuffer mainCommandBuffer;
    vk::Semaphore renderSemaphore;
    vk::Fence renderFence;
    vk::DescriptorSet descriptorSetUAB;
    vk::DescriptorSet descriptorSetRt;
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
    vk::DeviceAddress bufferAddress;
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
// struct MeshBuffer
// {
//     Buffer indexBuffer;
//     Buffer vertexBuffer;
//     // vk::DeviceAddress vertexBufferAddress;
//     // vk::DeviceAddress indexBufferAddress;
// };

// push constants for our mesh object draws
struct MeshPush
{
    uint32_t objId;
};

struct RayPush
{
    glm::vec4 clearColor;
    uint32_t numObjects;
    glm::vec3 lightPosition;
    float lightIntensity;
    uint32_t lightType;
};

// Per-object uniform buffer data
struct UniformData
{
    glm::mat4 worldMatrix;
};

// Per-object storage buffer data
struct ObjectStorageData
{
    vk::DeviceAddress vertexBufferAddress;
    vk::DeviceAddress indexBufferAddress;
};

// camera data for the storage buffer
struct CameraData
{
    glm::vec3 origin = glm::vec3(0.f);     // origin
    glm::vec3 orientation = glm::vec3(0.f, 0.f, 1.f);
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

