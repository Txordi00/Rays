#pragma once

#include "loader.hpp"
#include <glm/glm.hpp>


class Model
{
public:
    Model(const vk::Device &device, const HostMeshAsset &cpuMesh, const VmaAllocator &allocator);
    ~Model() = default;

    void updateModelMatrix();

    // void createGpuMesh(const vk::CommandBuffer &cmdTransfer,
    //                    const vk::Fence &transferFence,
    //                    const vk::Queue &transferQueue);

    void create_mesh(const vk::CommandBuffer &cmdTransfer,
                     const vk::Fence &transferFence,
                     const vk::Queue &transferQueue);

    void destroyBuffers();

    glm::vec3 position{0.f}, scale{1.f};
    float pitch{0.f}, yaw{0.f}, roll{0.f};
    glm::mat4 modelMatrix{1.f};

    // DeviceMeshAsset gpuMesh;
    std::string name;
    std::vector<GeoSurface> surfaces;

    Buffer indexBuffer;
    Buffer vertexBuffer;
    Buffer uniformBuffer;
    Buffer storageBuffer;

    uint32_t numVertices;
    uint32_t numIndices;

private:
    const vk::Device &device;
    const VmaAllocator &allocator;

    // void create_buffers();

    const void *verticesData;
    const void *indicesData;
};
