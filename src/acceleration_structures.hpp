#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan;
#endif

#include "loader.hpp"

struct AccelerationStructure
{
    vk::AccelerationStructureKHR AS;
    Buffer buffer;
    VkDeviceAddress addr;
};

struct TopLevelAS
{
    AccelerationStructure as;
    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    Buffer instancesBuffer;
};

class ASBuilder
{

public:
    ASBuilder(const vk::Device &device,
              const VmaAllocator &allocator,
              const uint32_t graphicsQueueFamilyIndex,
              const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties);
    ~ASBuilder() = default;
    void destroy();
    AccelerationStructure buildBLAS(const std::shared_ptr<MeshNode> &meshNode);

    TopLevelAS buildTLAS(const std::shared_ptr<GLTFObj> &scene);

    void updateTLAS(TopLevelAS &tlas, const glm::mat4 &transform);

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
    const uint32_t queueFamilyIndex;
    const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties;

    vk::CommandPool asPool;
    vk::Queue queue;
    vk::CommandBuffer asCmd;
    vk::Fence asFence;

    void init();

    std::vector<AccelerationStructure> blasQueue;
};
