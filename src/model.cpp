#include "model.hpp"
#include "types.hpp"
#include "utils.hpp"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <vk_mem_alloc.h>

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
                          vk::CommandBuffer &cmdTransfer,
                          vk::Fence &transferFence,
                          vk::Queue &transferQueue)
{
    gpuMesh.meshBuffer = create_mesh(device, allocator, cmdTransfer, transferFence, transferQueue);
    gpuMesh.name = name;
    gpuMesh.surfaces = cpuMesh.surfaces;
    buildBlasInput();
}

void Model::buildBlasInput()
{
    // Describe buffer as array of VertexObj.
    vk::AccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat); // vec3 vertex position data.
    triangles.setVertexData(vk::DeviceOrHostAddressConstKHR{gpuMesh.meshBuffer.vertexBufferAddress});
    triangles.setVertexStride(sizeof(Vertex));
    // Describe index data (32-bit unsigned int)
    triangles.setIndexType(vk::IndexType::eUint32);
    triangles.setIndexData(vk::DeviceOrHostAddressConstKHR{gpuMesh.meshBuffer.indexBufferAddress});
    // Indicate identity transform by setting transformData to null device pointer.
    //triangles.transformData = {};
    triangles.setMaxVertex(cpuMesh.vertices.size() - 1);

    // Identify the above data as containing opaque triangles.
    vk::AccelerationStructureGeometryKHR asGeom{};
    asGeom.setGeometryType(vk::GeometryTypeKHR::eTriangles);
    asGeom.setFlags(vk::GeometryFlagBitsKHR::eOpaque);
    asGeom.setGeometry(vk::AccelerationStructureGeometryDataKHR{triangles});

    // The entire array will be used to build the BLAS.
    uint32_t maxPrimitiveCount = cpuMesh.indices.size() / 3;
    vk::AccelerationStructureBuildRangeInfoKHR offset;
    offset.setFirstVertex(0);
    offset.setPrimitiveCount(maxPrimitiveCount);
    offset.setPrimitiveOffset(0);
    offset.setTransformOffset(0);

    // Our blas is made from only one geometry, but could be made of many geometries

    blasInput.asGeometry.emplace_back(asGeom);
    blasInput.asBuildRangeInfo.emplace_back(offset);
}

// OPTIMIZATION: This could be run on a separate thread in order to not force the main thread to wait
// for fences
MeshBuffer Model::create_mesh(const vk::Device &device,
                              const VmaAllocator &allocator,
                              vk::CommandBuffer &cmdTransfer,
                              vk::Fence &transferFence,
                              vk::Queue &transferQueue)
{
    const vk::DeviceSize verticesSize = cpuMesh.vertices.size() * sizeof(Vertex);
    const vk::DeviceSize indicesSize = cpuMesh.indices.size() * sizeof(uint32_t);

    MeshBuffer mesh;

    mesh.vertexBuffer = utils::create_buffer(allocator,
                                             verticesSize,
                                             vk::BufferUsageFlagBits::eStorageBuffer
                                                 | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                                 | vk::BufferUsageFlagBits::eTransferDst,
                                             VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    // | VMA_ALLOCATION_CREATE_MAPPED_BIT);
    vk::BufferDeviceAddressInfo vertexAddressInfo{};
    vertexAddressInfo.setBuffer(mesh.vertexBuffer.buffer);
    mesh.vertexBufferAddress = device.getBufferAddress(vertexAddressInfo);

    mesh.indexBuffer = utils::create_buffer(allocator,
                                            indicesSize,
                                            vk::BufferUsageFlagBits::eIndexBuffer
                                                | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                                | vk::BufferUsageFlagBits::eTransferDst,
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

    memcpy(stagingData, cpuMesh.vertices.data(), verticesSize);
    memcpy((char *) stagingData + verticesSize, cpuMesh.indices.data(), indicesSize);

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

void Model::cleanHost()
{
    cpuMesh = HostMeshAsset{};
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
