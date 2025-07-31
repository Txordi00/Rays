#pragma once
#include "model.hpp"

class ASBuilder
{
public:
    ASBuilder(const vk::Device &device,
              const VmaAllocator &allocator,
              const uint32_t graphicsQueueFamilyIndex);
    vk::AccelerationStructureKHR buildBLAS(const Model &model);

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
    const uint32_t queueFamilyIndex;

    vk::CommandPool asPool;
    vk::Queue queue;
    vk::CommandBuffer asCmd;

    void init();
};
