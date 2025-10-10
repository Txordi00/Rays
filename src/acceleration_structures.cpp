#include "acceleration_structures.hpp"
#include "utils.hpp"
#include <glm/gtc/type_ptr.hpp>

ASBuilder::ASBuilder(const vk::Device &device,
                     const VmaAllocator &allocator,
                     const uint32_t graphicsQueueFamilyIndex,
                     const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties)
    : device{device}
    , allocator{allocator}
    , queueFamilyIndex{graphicsQueueFamilyIndex}
    , asProperties{asProperties}
{
    init();
}

ASBuilder::~ASBuilder()
{
    device.freeCommandBuffers(asPool, asCmd);
    device.destroyFence(asFence);
    device.destroyCommandPool(asPool);
    if (blases.size() > 0) {
        for (auto &b : blases) {
            device.destroyAccelerationStructureKHR(b.AS);
            utils::destroy_buffer(allocator, b.buffer);
        }
    }
}

// NEED TO OPTIMIZE: Build for a vector of Model in batches, use a single (or a lower amount of)
// scractch buffers.
AccelerationStructure ASBuilder::buildBLAS(const std::shared_ptr<Model> &model)
{
    VK_CHECK_RES(device.waitForFences(asFence, vk::True, FENCE_TIMEOUT));
    device.resetFences(asFence);
    // Geometry description (single triangle array)
    vk::AccelerationStructureGeometryTrianglesDataKHR triData{};
    triData.setVertexFormat(vk::Format::eR32G32B32Sfloat);
    triData.setVertexData(vk::DeviceOrHostAddressConstKHR{model->vertexBuffer.bufferAddress});
    triData.setVertexStride(sizeof(Vertex));
    triData.setMaxVertex(model->numVertices - 1); // conservative
    triData.setIndexType(vk::IndexType::eUint32);
    triData.setIndexData(vk::DeviceOrHostAddressConstKHR{model->indexBuffer.bufferAddress});

    vk::AccelerationStructureGeometryKHR geom{};
    geom.setGeometryType(vk::GeometryTypeKHR::eTriangles);
    // geom.setFlags(vk::GeometryFlagBitsKHR::eOpaque); // simplest
    geom.setGeometry(triData);

    // The entire array will be used to build the BLAS.
    vk::AccelerationStructureBuildRangeInfoKHR offsets{};
    offsets.setFirstVertex(0);
    offsets.setPrimitiveCount(model->numIndices / 3);
    offsets.setPrimitiveOffset(0);
    offsets.setTransformOffset(0);

    // Info necessary to get the vk::AccelerationStructureBuildSizesInfoKHR struct
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
    buildInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    buildInfo.setGeometries(geom);

    vk::AccelerationStructureBuildSizesInfoKHR buildSizes
        = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                       buildInfo,
                                                       offsets.primitiveCount);

    // Allocate scratch buffer
    Buffer scratchBuffer
        = utils::create_buffer(device,
                               allocator,
                               buildSizes.buildScratchSize,
                               vk::BufferUsageFlagBits::eStorageBuffer
                                   | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               0,
                               asProperties.minAccelerationStructureScratchOffsetAlignment);

    AccelerationStructure blas;
    blas.buffer = utils::create_buffer(device,
                                       allocator,
                                       buildSizes.accelerationStructureSize,
                                       vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                                           | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                       VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                       0,
                                       asProperties.minAccelerationStructureScratchOffsetAlignment);

    // Actual allocation of buffer and acceleration structure.
    vk::AccelerationStructureCreateInfoKHR blasCreate{};
    blasCreate.setSize(buildSizes.accelerationStructureSize);
    blasCreate.setOffset(0);
    blasCreate.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
    // blasCreate.setCreateFlags();
    blasCreate.setBuffer(blas.buffer.buffer);
    // blasCreate.setDeviceAddress(); // BLAS address is not the same as blas-buffer address!

    blas.AS = device.createAccelerationStructureKHR(blasCreate);

    vk::AccelerationStructureDeviceAddressInfoKHR blasAddressInfo{};
    blasAddressInfo.setAccelerationStructure(blas.AS);
    blas.addr = device.getAccelerationStructureAddressKHR(blasAddressInfo);

    buildInfo.setSrcAccelerationStructure(nullptr);
    buildInfo.setDstAccelerationStructure(blas.AS);
    buildInfo.setScratchData(vk::DeviceOrHostAddressKHR{scratchBuffer.bufferAddress});

    // Record and submit the command buffer
    utils::cmd_submit(device, queue, asFence, asCmd, [&](const vk::CommandBuffer &cmd) {
        cmd.buildAccelerationStructuresKHR(buildInfo, &offsets);
        // Barrier: BLAS build writes → TLAS read later
        vk::MemoryBarrier2 barrier{};
        barrier.setSrcAccessMask(vk::AccessFlagBits2::eAccelerationStructureWriteKHR);
        barrier.setDstAccessMask(vk::AccessFlagBits2::eAccelerationStructureReadKHR);
        barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR);
        barrier.setDstStageMask(vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR);
        vk::DependencyInfo barrierInfo{};
        barrierInfo.setMemoryBarriers(barrier);
        cmd.pipelineBarrier2(barrierInfo);
    });

    // Scratch buffer can be destroyed after queue finishes
    utils::destroy_buffer(allocator, scratchBuffer);

    return blas;
}

// NEED TO OPTIMIZE: Do instances of a single model.
AccelerationStructure ASBuilder::buildTLAS(const std::vector<AccelerationStructure> &blases,
                                           const std::vector<glm::mat3x4> &transforms)
{
    assert(blases.size() == transforms.size());
    VK_CHECK_RES(device.waitForFences(asFence, vk::True, FENCE_TIMEOUT));
    device.resetFences(asFence);

    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    instances.reserve(blases.size());
    for (uint32_t i = 0; i < blases.size(); i++) {
        glm::mat3x4 transformGlm = transforms[i];
        AccelerationStructure blas = blases[i];
        vk::AccelerationStructureInstanceKHR instance{};
        vk::TransformMatrixKHR transformVk;
        memcpy(&transformVk, glm::value_ptr(transformGlm), sizeof(vk::TransformMatrixKHR));
        instance.setTransform(transformVk);
        instance.setInstanceCustomIndex(i); // gl_InstanceCustomIndexEXT
        instance.setAccelerationStructureReference(blas.addr);
        // instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque);
        instance.setMask(0xFF); //  Only be hit if rayMask & instance.mask != 0
        instance.setInstanceShaderBindingTableRecordOffset(
            0); // We will use the same hit group for all objects
        instances.emplace_back(instance);
    }

    asCmd.reset();

    // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
    vk::DeviceSize instancesSize = static_cast<vk::DeviceSize>(
        sizeof(vk::AccelerationStructureInstanceKHR) * instances.size());
    // Buffer of instances containing the matrices and BLAS ids
    Buffer instancesBuffer
        = utils::create_buffer(device,
                               allocator,
                               instancesSize,
                               vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
                                   | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                   | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Fill the buffer
    memcpy(instancesBuffer.allocationInfo.pMappedData, instances.data(), instancesSize);

    // Wraps a device pointer to the above uploaded instances.
    vk::AccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.setData(vk::DeviceOrHostAddressConstKHR{instancesBuffer.bufferAddress});

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    vk::AccelerationStructureGeometryKHR topASGeometry{};
    topASGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
    topASGeometry.setGeometry(instancesData);
    topASGeometry.setFlags(vk::GeometryFlagBitsKHR::eOpaque);

    // Find sizes
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    buildInfo.setGeometries(topASGeometry);
    buildInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
    buildInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
    buildInfo.setSrcAccelerationStructure(nullptr);

    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo
        = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                       buildInfo,
                                                       instances.size());

    // Create acceleration structure, not building it yet
    // NOT GETTING THE SHADER ADDRESS AT THE MOMENT
    AccelerationStructure tlas;
    tlas.buffer = utils::create_buffer(device,
                                       allocator,
                                       sizeInfo.accelerationStructureSize,
                                       vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
                                       VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                       0);

    // 4. Create the acceleration structure object
    vk::AccelerationStructureCreateInfoKHR asInfo{};
    asInfo.setBuffer(tlas.buffer.buffer);
    asInfo.setSize(sizeInfo.accelerationStructureSize);
    asInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);

    tlas.AS = device.createAccelerationStructureKHR(asInfo);

    // For now we do not need its device address
    // vk::AccelerationStructureDeviceAddressInfoKHR tlasAddressInfo{};
    // tlasAddressInfo.setAccelerationStructure(tlas.AS);
    // tlas.addr = device.getAccelerationStructureAddressKHR(tlasAddressInfo);

    // Allocate scratch buffer
    Buffer scratchBuffer
        = utils::create_buffer(device,
                               allocator,
                               sizeInfo.buildScratchSize,
                               vk::BufferUsageFlagBits::eStorageBuffer
                                   | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               0,
                               asProperties.minAccelerationStructureScratchOffsetAlignment);

    // Update build information
    buildInfo.setSrcAccelerationStructure(nullptr);
    buildInfo.setDstAccelerationStructure(tlas.AS);
    buildInfo.setScratchData(scratchBuffer.bufferAddress);

    // Build Offsets info: n instances
    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.setPrimitiveCount(instances.size());

    // Build the TLAS
    // Record the next set of commands
    utils::cmd_submit(device, queue, asFence, asCmd, [&](const vk::CommandBuffer &cmd) {
        cmd.buildAccelerationStructuresKHR(buildInfo, &buildRangeInfo);
    });

    // Scratch buffer can be destroyed after queue finishes
    utils::destroy_buffer(allocator, scratchBuffer);
    utils::destroy_buffer(allocator, instancesBuffer);

    return tlas;
}

AccelerationStructure ASBuilder::buildTLAS(const std::vector<std::shared_ptr<Model> > &models,
                                           const std::vector<glm::mat3x4> &transforms)
{
    blases.clear();
    blases.reserve(models.size());
    for (const auto &m : models)
        blases.emplace_back(buildBLAS(m));

    return buildTLAS(blases, transforms);
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
    queueInfo.setQueueIndex(0); // Why can't I use index 1?
    queue = device.getQueue2(queueInfo);

    vk::CommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.setCommandPool(asPool);
    cmdAllocInfo.setCommandBufferCount(1);
    cmdAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    asCmd = device.allocateCommandBuffers(cmdAllocInfo)[0];

    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
    asFence = device.createFence(fenceInfo);
}
