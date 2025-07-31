#include "ray_tracing.hpp"
#include "utils.hpp"

ASBuilder::ASBuilder(const vk::Device &device,
                     const VmaAllocator &allocator,
                     const uint32_t graphicsQueueFamilyIndex)
    : device{device}
    , allocator{allocator}
    , queueFamilyIndex{graphicsQueueFamilyIndex}
{
    init();
}

vk::AccelerationStructureKHR ASBuilder::buildBLAS(const Model &model)
{
    // 1. Geometry description (single triangle array)
    vk::AccelerationStructureGeometryTrianglesDataKHR triData{};
    triData.setVertexFormat(vk::Format::eR32G32B32Sfloat);
    triData.setVertexData(
        vk::DeviceOrHostAddressConstKHR{model.gpuMesh.meshBuffer.vertexBufferAddress});
    triData.setVertexStride(sizeof(Vertex));
    triData.setMaxVertex(model.cpuMesh.vertices.size() - 1); // conservative
    triData.setIndexType(vk::IndexType::eUint32);
    triData.setIndexData(
        vk::DeviceOrHostAddressConstKHR{model.gpuMesh.meshBuffer.indexBufferAddress});

    vk::AccelerationStructureGeometryKHR geom{};
    geom.setGeometryType(vk::GeometryTypeKHR::eTriangles);
    geom.setFlags(vk::GeometryFlagBitsKHR::eOpaque); // simplest
    geom.setGeometry(triData);

    // 2. Get build sizes
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    buildInfo.setGeometries(geom);

    uint32_t primCount = model.cpuMesh.indices.size() / 3;
    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo
        = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                       buildInfo,
                                                       primCount);

    // 3. Allocate BLAS buffer
    Buffer blasBuffer = utils::create_buffer(allocator,
                                             sizeInfo.accelerationStructureSize,
                                             vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                                                 | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                             VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                             0);

    vk::BufferDeviceAddressInfo blasAddrInfo{};
    blasAddrInfo.setBuffer(blasBuffer.buffer);
    vk::DeviceAddress blasAdrr = device.getBufferAddress(blasAddrInfo);

    // 4. Create the acceleration structure object
    vk::AccelerationStructureCreateInfoKHR asInfo{};
    asInfo.setBuffer(blasBuffer.buffer);
    asInfo.setSize(sizeInfo.accelerationStructureSize);
    asInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

    vk::AccelerationStructureKHR blas = device.createAccelerationStructureKHR(asInfo);

    // 5. Allocate scratch buffer
    Buffer scratchBuffer = utils::create_buffer(allocator,
                                                sizeInfo.buildScratchSize,
                                                vk::BufferUsageFlagBits::eStorageBuffer
                                                    | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                                0);
    vk::BufferDeviceAddressInfo scratchAddrInfo{};
    scratchAddrInfo.setBuffer(scratchBuffer.buffer);
    vk::DeviceAddress scratchAddr = device.getBufferAddress(scratchAddrInfo);

    // 6. Build command
    buildInfo.setDstAccelerationStructure(blas);
    buildInfo.setScratchData(vk::DeviceOrHostAddressKHR{scratchAddr});

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = primCount;

    const VkAccelerationStructureBuildRangeInfoKHR *pRange = &range;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    // 7. Barrier: BLAS build writes → TLAS read later
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0,
                         1,
                         &barrier,
                         0,
                         nullptr,
                         0,
                         nullptr);

    // 8. Device address (needed by TLAS)
    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addrInfo.accelerationStructure = blas;
    VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);

    // you may store blasAddr in a struct for TLAS creation later

    // 9. scratch buffer can be destroyed after queue finishes
    // (queue submit & fence not shown for brevity)

    return blas;
}

void ASBuilder::init()
{
    vk::CommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer
                                   | vk::CommandPoolCreateFlagBits::eTransient);
    commandPoolCreateInfo.setQueueFamilyIndex(queueFamilyIndex);
    asPool = device.createCommandPool(commandPoolCreateInfo);
    vk::DeviceQueueInfo2 queueInfo{};
    queueInfo.setQueueFamilyIndex(queueFamilyIndex);
    // queueInfo.setQueueIndex(1);
    queue = device.getQueue2(queueInfo);

    vk::CommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.setCommandPool(asPool);
    cmdAllocInfo.setCommandBufferCount(1);
    cmdAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    asCmd = device.allocateCommandBuffers(cmdAllocInfo)[0];
}
