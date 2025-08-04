#pragma once
#include "model.hpp"

class ASBuilder
{
    struct Blas
    {
        vk::AccelerationStructureKHR blasAS;
        Buffer blasBuffer;
        VkDeviceAddress blasAddr;
    };

public:
    ASBuilder(const vk::Device &device,
              const VmaAllocator &allocator,
              const uint32_t graphicsQueueFamilyIndex,
              const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties);
    ~ASBuilder();
    ASBuilder::Blas buildBLAS(const Model &model);

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
    const uint32_t queueFamilyIndex;
    const vk::PhysicalDeviceAccelerationStructurePropertiesKHR asProperties;

    vk::CommandPool asPool;
    vk::Queue queue;
    vk::CommandBuffer asCmd;

    void init();
};
