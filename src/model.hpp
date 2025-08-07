#pragma once

#include "loader.hpp"
#include <glm/glm.hpp>


class Model
{
    // struct BlasInput
    // {
    //     // Data used to build acceleration structure geometry
    //     std::vector<vk::AccelerationStructureGeometryKHR> asGeometry;
    //     std::vector<vk::AccelerationStructureBuildRangeInfoKHR> asBuildRangeInfo;
    //     vk::BuildAccelerationStructureFlagsKHR flags{};
    // };

public:
    Model(const HostMeshAsset &cpuMesh);
    ~Model() = default;

    void updateModelMatrix();

    void createGpuMesh(const vk::Device &device,
                       const VmaAllocator &allocator,
                       vk::CommandBuffer &cmdTransfer,
                       vk::Fence &transferFence,
                       vk::Queue &transferQueue);

    // void cleanHost();

    void destroyBuffers(const VmaAllocator &allocator);

    glm::vec3 position{0.f}, scale{1.f};
    float pitch{0.f}, yaw{0.f}, roll{0.f};
    glm::mat4 modelMatrix{1.f};

    DeviceMeshAsset gpuMesh;
    // BlasInput blasInput;
    std::string name;

    uint32_t numVertices;
    uint32_t numIndices;

private:
    MeshBuffer create_mesh(const vk::Device &device,
                           const VmaAllocator &allocator,
                           vk::CommandBuffer &cmdTransfer,
                           vk::Fence &transferFence,
                           vk::Queue &transferQueue);

    const void *verticesData;
    const void *indicesData;

    // void buildBlasInput();
};
