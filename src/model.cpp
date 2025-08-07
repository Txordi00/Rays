#include "model.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_mem_alloc.h>

Model::Model(const HostMeshAsset &cpuMesh)
{
    name = cpuMesh.name;
    numVertices = cpuMesh.vertices.size();
    numIndices = cpuMesh.indices.size();
    gpuMesh.surfaces = cpuMesh.surfaces;
    verticesData = cpuMesh.vertices.data();
    indicesData = cpuMesh.indices.data();
}

void Model::updateModelMatrix()
{
    // The order is translate -> rotate -> scale.
    glm::mat4 transMat = glm::translate(glm::mat4(1.f), position);
    // I implement the rotations as quaternions and I accumulate them in quaternion space
    glm::quat pQuat = glm::angleAxis(pitch, glm::vec3(1, 0, 0));
    glm::quat yQuat = glm::angleAxis(yaw, glm::vec3(0, 1, 0));
    glm::quat rQuat = glm::angleAxis(roll, glm::vec3(0, 0, 1));
    // After that I generate the rotation matrix with them. Would be more efficient to send directly
    // the quaternions + the translations to the shader?
    glm::mat4 rotMat = glm::toMat4(pQuat * yQuat * rQuat);

    glm::mat4 scaleMat = glm::scale(scale);

    // This becomes modelMat = T * R
    modelMatrix = transMat * rotMat * scaleMat;
}

void Model::createGpuMesh(const vk::Device &device,
                          const VmaAllocator &allocator,
                          const vk::CommandBuffer &cmdTransfer,
                          const vk::Fence &transferFence,
                          const vk::Queue &transferQueue)
{
    gpuMesh.meshBuffer = create_mesh(device, allocator, cmdTransfer, transferFence, transferQueue);
    gpuMesh.name = name;
}


// OPTIMIZATION: This could be run on a separate thread in order to not force the main thread to wait
// for fences
MeshBuffer Model::create_mesh(const vk::Device &device,
                              const VmaAllocator &allocator,
                              const vk::CommandBuffer &cmdTransfer,
                              const vk::Fence &transferFence,
                              const vk::Queue &transferQueue)
{
    const vk::DeviceSize verticesSize = numVertices * sizeof(Vertex);
    const vk::DeviceSize indicesSize = numIndices * sizeof(uint32_t);

    MeshBuffer mesh;

    mesh.vertexBuffer = utils::create_buffer(
        allocator,
        verticesSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    // | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    vk::BufferDeviceAddressInfo vertexAddressInfo{};
    vertexAddressInfo.setBuffer(mesh.vertexBuffer.buffer);
    mesh.vertexBufferAddress = device.getBufferAddress(vertexAddressInfo);

    mesh.indexBuffer = utils::create_buffer(
        allocator,
        indicesSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0);
    vk::BufferDeviceAddressInfo indexAddressInfo{};
    indexAddressInfo.setBuffer(mesh.indexBuffer.buffer);
    mesh.indexBufferAddress = device.getBufferAddress(indexAddressInfo);

    Buffer stagingBuffer = utils::create_buffer(allocator,
                                                verticesSize + indicesSize,
                                                vk::BufferUsageFlagBits::eTransferSrc,
                                                VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                    | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void *stagingData = stagingBuffer.allocationInfo.pMappedData;

    memcpy(stagingData, verticesData, verticesSize);
    memcpy((char *) stagingData + verticesSize, indicesData, indicesSize);

    // Set info structures to copy from staging to vertex & index buffers
    vk::CopyBufferInfo2 vertexCopyInfo{};
    vertexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
    vertexCopyInfo.setDstBuffer(mesh.vertexBuffer.buffer);
    vk::BufferCopy2 vertexCopy{};
    vertexCopy.setSrcOffset(0);
    vertexCopy.setDstOffset(0);
    vertexCopy.setSize(verticesSize);
    vertexCopyInfo.setRegions(vertexCopy);

    vk::CopyBufferInfo2 indexCopyInfo{};
    indexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
    indexCopyInfo.setDstBuffer(mesh.indexBuffer.buffer);
    vk::BufferCopy2 indexCopy{};
    indexCopy.setSrcOffset(verticesSize);
    indexCopy.setDstOffset(0);
    indexCopy.setSize(indicesSize);
    indexCopyInfo.setRegions(indexCopy);

    // New command buffer to copy in GPU from staging buffer to vertex & index buffers
    device.resetFences(transferFence);

    vk::CommandBufferBeginInfo cmdBeginInfo{};
    cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    cmdTransfer.begin(cmdBeginInfo);
    cmdTransfer.copyBuffer2(vertexCopyInfo);
    cmdTransfer.copyBuffer2(indexCopyInfo);
    cmdTransfer.end();

    vk::CommandBufferSubmitInfo cmdSubmitInfo{};
    cmdSubmitInfo.setCommandBuffer(cmdTransfer);
    cmdSubmitInfo.setDeviceMask(1);
    vk::SubmitInfo2 submitInfo{};
    submitInfo.setCommandBufferInfos(cmdSubmitInfo);

    try {
        transferQueue.submit2(submitInfo, transferFence);
        VK_CHECK_RES(device.waitForFences(transferFence, vk::True, FENCE_TIMEOUT));
    } catch (const std::exception &e) {
        VK_CHECK_EXC(e);
    }
    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);

    return mesh;
}

void Model::destroyBuffers(const VmaAllocator &allocator)
{
    vmaDestroyBuffer(allocator,
                     gpuMesh.meshBuffer.vertexBuffer.buffer,
                     gpuMesh.meshBuffer.vertexBuffer.allocation);
    vmaDestroyBuffer(allocator,
                     gpuMesh.meshBuffer.indexBuffer.buffer,
                     gpuMesh.meshBuffer.indexBuffer.allocation);
}
