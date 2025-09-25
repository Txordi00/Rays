#pragma once
#include "model.hpp"

struct AccelerationStructure
{
    vk::AccelerationStructureKHR AS;
    Buffer buffer;
    VkDeviceAddress addr;
};

class ASBuilder
{

public:
    ASBuilder(const vk::Device &device,
              const VmaAllocator &allocator,
              const uint32_t graphicsQueueFamilyIndex,
              const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties);
    ~ASBuilder();
    AccelerationStructure buildBLAS(const std::shared_ptr<Model> &model);
    AccelerationStructure buildTLAS(const std::vector<AccelerationStructure> &blases,
                                    const std::vector<glm::mat3x4> &transforms);

    AccelerationStructure buildTLAS(const std::vector<std::shared_ptr<Model>> &models,
                                    const std::vector<glm::mat3x4> &transforms);

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

    std::vector<AccelerationStructure> blases;
};
