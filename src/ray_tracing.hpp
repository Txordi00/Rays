#pragma once
#include "model.hpp"

struct Blas
{
    vk::AccelerationStructureKHR blasAS;
    Buffer blasBuffer;
    VkDeviceAddress blasAddr;
};

class ASBuilder
{

public:
    ASBuilder(const vk::Device &device,
              const VmaAllocator &allocator,
              const uint32_t graphicsQueueFamilyIndex,
              const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties);
    ~ASBuilder();
    Blas buildBLAS(const std::shared_ptr<Model> &model);
    void buildTLAS(const std::vector<Blas> &blases);

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
