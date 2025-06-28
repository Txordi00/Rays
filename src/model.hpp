#pragma once

#include "loader.hpp"
#include <glm/glm.hpp>


class Model
{
public:
    Model(HostMeshAsset &cpuMesh)
        : cpuMesh{cpuMesh}
        , name{cpuMesh.name} {};
    ~Model() = default;

    void updateModelMatrix();

    void createGpuMesh(const vk::Device &device,
                       const VmaAllocator &allocator,
                       vk::CommandBuffer &cmdTransfer,
                       vk::Fence &transferFence,
                       vk::Queue &transferQueue);

    MeshBuffer create_mesh(const vk::Device &device,
                           const VmaAllocator &allocator,
                           vk::CommandBuffer &cmdTransfer,
                           vk::Fence &transferFence,
                           vk::Queue &transferQueue);
    void cleanHost();

    void destroyBuffers(const VmaAllocator &allocator);

    glm::vec3 position{0.f}, scale{1.f};
    float pitch{0.f}, yaw{0.f}, roll{0.f};
    glm::mat4 modelMatrix{1.f};

    DeviceMeshAsset gpuMesh;
    std::string name;

private:
    HostMeshAsset &cpuMesh;
};
