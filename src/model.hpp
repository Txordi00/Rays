#pragma once

#include "loader.hpp"
#include <glm/glm.hpp>


class Model
{
public:
    Model(const HostMeshAsset &cpuMesh, const VmaAllocator &allocator);
    ~Model() = default;

    void updateModelMatrix();

    void createGpuMesh(const vk::Device &device,
                       const vk::CommandBuffer &cmdTransfer,
                       const vk::Fence &transferFence,
                       const vk::Queue &transferQueue);

    void destroyBuffers();

    glm::vec3 position{0.f}, scale{1.f};
    float pitch{0.f}, yaw{0.f}, roll{0.f};
    glm::mat4 modelMatrix{1.f};

    DeviceMeshAsset gpuMesh;
    std::string name;

    Buffer uniformBuffer;

    uint32_t numVertices;
    uint32_t numIndices;

private:
    const VmaAllocator &allocator;
    MeshBuffer create_mesh(const vk::Device &device,
                           const VmaAllocator &allocator,
                           const vk::CommandBuffer &cmdTransfer,
                           const vk::Fence &transferFence,
                           const vk::Queue &transferQueue);

    void allocate_uniform_buffer();

    const void *verticesData;
    const void *indicesData;
};
