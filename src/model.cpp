#include "model.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_mem_alloc.h>

Model::Model(const vk::Device &device, const HostMeshAsset &cpuMesh, const VmaAllocator &allocator)
    : device{device}
    , allocator{allocator}
{
    name = cpuMesh.name;
    numVertices = cpuMesh.vertices.size();
    numIndices = cpuMesh.indices.size();
    surfaces = cpuMesh.surfaces;
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

// OPTIMIZATION: This could be run on a separate thread in order to not force the main thread to wait
// for fences
void Model::create_mesh(const vk::CommandBuffer &cmdTransfer,
                        const vk::Fence &transferFence,
                        const vk::Queue &transferQueue)
{
    const vk::DeviceSize verticesSize = numVertices * sizeof(Vertex);
    const vk::DeviceSize indicesSize = numIndices * sizeof(uint32_t);

    vertexBuffer = utils::create_buffer(
        device,
        allocator,
        verticesSize,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0);
    // | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    // vk::BufferDeviceAddressInfo vertexAddressInfo{};
    // vertexAddressInfo.setBuffer(mesh.vertexBuffer.buffer);
    // mesh.vertexBufferAddress = device.getBufferAddress(vertexAddressInfo);

    indexBuffer = utils::create_buffer(
        device,
        allocator,
        indicesSize,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eStorageBuffer
            | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0);
    // vk::BufferDeviceAddressInfo indexAddressInfo{};
    // indexAddressInfo.setBuffer(mesh.indexBuffer.buffer);
    // mesh.indexBufferAddress = device.getBufferAddress(indexAddressInfo);

    Buffer stagingBuffer = utils::create_buffer(device,
                                                allocator,
                                                verticesSize + indicesSize,
                                                vk::BufferUsageFlagBits::eTransferSrc,
                                                VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                    | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    void *stagingData = stagingBuffer.allocationInfo.pMappedData;
    assert(stagingData && "Staging buffer must be mapped");

    memcpy(stagingData, verticesData, verticesSize);
    memcpy((char *) stagingData + verticesSize, indicesData, indicesSize);

    // Set info structures to copy from staging to vertex & index buffers
    vk::BufferCopy2 vertexCopy{};
    vertexCopy.setSrcOffset(0);
    vertexCopy.setDstOffset(0);
    vertexCopy.setSize(verticesSize);
    vk::CopyBufferInfo2 vertexCopyInfo{};
    vertexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
    vertexCopyInfo.setDstBuffer(vertexBuffer.buffer);
    vertexCopyInfo.setRegions(vertexCopy);

    vk::BufferCopy2 indexCopy{};
    indexCopy.setSrcOffset(verticesSize);
    indexCopy.setDstOffset(0);
    indexCopy.setSize(indicesSize);
    vk::CopyBufferInfo2 indexCopyInfo{};
    indexCopyInfo.setSrcBuffer(stagingBuffer.buffer);
    indexCopyInfo.setDstBuffer(indexBuffer.buffer);
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

    // Create the per-model uniform & storage buffers
    uniformBuffer = utils::create_buffer(device,
                                         allocator,
                                         vk::DeviceSize(sizeof(UniformData)),
                                         vk::BufferUsageFlagBits::eUniformBuffer,
                                         VMA_MEMORY_USAGE_AUTO,
                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                             | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    storageBuffer = utils::create_buffer(device,
                                         allocator,
                                         vk::DeviceSize(sizeof(ObjectStorageData)),
                                         vk::BufferUsageFlagBits::eStorageBuffer,
                                         VMA_MEMORY_USAGE_AUTO,
                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                             | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Pass the buffer addresses to the storage buffer from the get-go
    ObjectStorageData objectStorage;
    objectStorage.vertexBufferAddress = vertexBuffer.bufferAddress;
    objectStorage.indexBufferAddress = indexBuffer.bufferAddress;
    objectStorage.numVertices = numVertices;
    objectStorage.numIndices = numIndices;
    utils::map_to_buffer(storageBuffer, &objectStorage);
}

void Model::destroyBuffers()
{
    utils::destroy_buffer(allocator, uniformBuffer);
    utils::destroy_buffer(allocator, storageBuffer);
    utils::destroy_buffer(allocator, vertexBuffer);
    utils::destroy_buffer(allocator, indexBuffer);
}
