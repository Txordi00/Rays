#pragma once

#include "loader.hpp"
#include <glm/glm.hpp>


class Model
{
public:
    Model(const HostMeshAsset &cpuMesh);
    ~Model() = default;

    void updateModelMatrix();

    void createGpuMesh(const vk::Device &device,
                       const VmaAllocator &allocator,
                       const vk::CommandBuffer &cmdTransfer,
                       const vk::Fence &transferFence,
                       const vk::Queue &transferQueue);

    void destroyBuffers(const VmaAllocator &allocator);

    glm::vec3 position{0.f}, scale{1.f};
    float pitch{0.f}, yaw{0.f}, roll{0.f};
    glm::mat4 modelMatrix{1.f};

    DeviceMeshAsset gpuMesh;
    std::string name;

    uint32_t numVertices;
    uint32_t numIndices;

private:
    MeshBuffer create_mesh(const vk::Device &device,
                           const VmaAllocator &allocator,
                           const vk::CommandBuffer &cmdTransfer,
                           const vk::Fence &transferFence,
                           const vk::Queue &transferQueue);

    const void *verticesData;
    const void *indicesData;
};
