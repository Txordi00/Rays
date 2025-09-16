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
    // 1. Geometry description (single triangle array)
    vk::AccelerationStructureGeometryTrianglesDataKHR triData{};
    triData.setVertexFormat(vk::Format::eR32G32B32Sfloat);
    triData.setVertexData(vk::DeviceOrHostAddressConstKHR{model->vertexBuffer.bufferAddress});
    triData.setVertexStride(sizeof(Vertex));
    triData.setMaxVertex(model->numVertices - 1); // conservative
    triData.setIndexType(vk::IndexType::eUint32);
    triData.setIndexData(vk::DeviceOrHostAddressConstKHR{model->indexBuffer.bufferAddress});

    vk::AccelerationStructureGeometryKHR geom{};
    geom.setGeometryType(vk::GeometryTypeKHR::eTriangles);
    geom.setFlags(vk::GeometryFlagBitsKHR::eOpaque); // simplest
    geom.setGeometry(triData);

    // 2. Get build sizes
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    buildInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
    buildInfo.setGeometries(geom);

    uint32_t primCount = model->numIndices / 3;
    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo
        = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                       buildInfo,
                                                       primCount);

    // 3. Allocate BLAS buffer
    Buffer blasBuffer = utils::create_buffer(device,
                                             allocator,
                                             sizeInfo.accelerationStructureSize,
                                             vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR
                                                 | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                             VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                             0);

    // 4. Create the acceleration structure object
    vk::AccelerationStructureCreateInfoKHR asInfo{};
    asInfo.setBuffer(blasBuffer.buffer);
    asInfo.setSize(sizeInfo.accelerationStructureSize);
    asInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);

    vk::AccelerationStructureKHR blas = device.createAccelerationStructureKHR(asInfo);

    // 5. Allocate scratch buffer
    Buffer scratchBuffer
        = utils::create_buffer(device,
                               allocator,
                               sizeInfo.buildScratchSize,
                               vk::BufferUsageFlagBits::eStorageBuffer
                                   | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               0,
                               asProperties.minAccelerationStructureScratchOffsetAlignment);
    vk::BufferDeviceAddressInfo scratchAddrInfo{};
    scratchAddrInfo.setBuffer(scratchBuffer.buffer);
    vk::DeviceAddress scratchAddr = device.getBufferAddress(scratchAddrInfo);

    // 6. Build command
    buildInfo.setDstAccelerationStructure(blas);
    buildInfo.setScratchData(vk::DeviceOrHostAddressKHR{scratchAddr});

    vk::AccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.setPrimitiveCount(primCount);

    asCmd.reset();

    // Record the next set of commands
    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    asCmd.begin(commandBufferBeginInfo);

    asCmd.buildAccelerationStructuresKHR(buildInfo, &rangeInfo);

    // 7. Barrier: BLAS build writes → TLAS read later
    vk::MemoryBarrier barrier{};
    barrier.setSrcAccessMask(vk::AccessFlagBits::eAccelerationStructureWriteKHR);
    barrier.setDstAccessMask(vk::AccessFlagBits::eAccelerationStructureReadKHR);
    // vk::DependencyInfo depInfo{};
    // asCmd.pipelineBarrier2(depInfo)
    asCmd.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
                          vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR,
                          vk::DependencyFlags{},
                          barrier,
                          nullptr,
                          nullptr);

    // 8. Device address (needed by TLAS)
    vk::AccelerationStructureDeviceAddressInfoKHR blasAddrInfo{};
    blasAddrInfo.setAccelerationStructure(blas);
    VkDeviceAddress blasAddr = device.getAccelerationStructureAddressKHR(blasAddrInfo);

    // Queue submit
    asCmd.end();
    vk::SubmitInfo2 submitInfo{};
    vk::CommandBufferSubmitInfo cmdInfo{asCmd, 1};
    submitInfo.setCommandBufferInfos(cmdInfo);
    queue.submit2(submitInfo, asFence);

    // 9. scratch buffer can be destroyed after queue finishes
    utils::destroy_buffer(allocator, scratchBuffer);

    return AccelerationStructure{blas, blasBuffer, blasAddr};
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
    for (int i = 0; i < blases.size(); i++) {
        glm::mat3x4 transformGlm = transforms[i];
        AccelerationStructure blas = blases[i];
        vk::AccelerationStructureInstanceKHR instance{};
        vk::TransformMatrixKHR transformVk;
        memcpy(&transformVk, glm::value_ptr(transformGlm), sizeof(vk::TransformMatrixKHR));
        instance.setTransform(transformVk);
        instance.setInstanceCustomIndex(i); // gl_InstanceCustomIndexEXT
        instance.setAccelerationStructureReference(blas.addr);
        // instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque
        //                   | vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable
        //                   | vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);
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

    vk::BufferDeviceAddressInfo bufferInfo{instancesBuffer.buffer};
    vk::DeviceAddress instBufferAddr = device.getBufferAddress(bufferInfo);

    // Fill the buffer
    memcpy(instancesBuffer.allocationInfo.pMappedData, instances.data(), instancesSize);

    // Wraps a device pointer to the above uploaded instances.
    vk::AccelerationStructureGeometryInstancesDataKHR instancesGeom{};
    instancesGeom.setData(vk::DeviceOrHostAddressConstKHR{instBufferAddr});

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    vk::AccelerationStructureGeometryKHR topASGeometry{};
    topASGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
    topASGeometry.setGeometry(instancesGeom);

    // Find sizes
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
                       | vk::BuildAccelerationStructureFlagBitsKHR::eAllowDataAccess);
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
    Buffer tlasBuffer
        = utils::create_buffer(device,
                               allocator,
                               sizeInfo.accelerationStructureSize,
                               vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               0);

    // 4. Create the acceleration structure object
    vk::AccelerationStructureCreateInfoKHR asInfo{};
    asInfo.setBuffer(tlasBuffer.buffer);
    asInfo.setSize(sizeInfo.accelerationStructureSize);
    asInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);

    vk::AccelerationStructureKHR tlas = device.createAccelerationStructureKHR(asInfo);

    // 5. Allocate scratch buffer
    Buffer scratchBuffer
        = utils::create_buffer(device,
                               allocator,
                               sizeInfo.buildScratchSize,
                               vk::BufferUsageFlagBits::eStorageBuffer
                                   | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               0,
                               asProperties.minAccelerationStructureScratchOffsetAlignment);
    vk::BufferDeviceAddressInfo scratchAddrInfo{};
    scratchAddrInfo.setBuffer(scratchBuffer.buffer);
    vk::DeviceAddress scratchAddr = device.getBufferAddress(scratchAddrInfo);

    // Update build information
    buildInfo.setSrcAccelerationStructure(nullptr);
    buildInfo.setDstAccelerationStructure(tlas);
    buildInfo.setScratchData(vk::DeviceOrHostAddressKHR{scratchAddr});

    // Build Offsets info: n instances
    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.setPrimitiveCount(instances.size());

    // Build the TLAS
    // Record the next set of commands
    vk::CommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    asCmd.begin(commandBufferBeginInfo);

    asCmd.buildAccelerationStructuresKHR(buildInfo, &buildRangeInfo);

    asCmd.end();
    vk::SubmitInfo2 submitInfo{};
    vk::CommandBufferSubmitInfo cmdInfo{asCmd, 1};
    submitInfo.setCommandBufferInfos(cmdInfo);
    queue.submit2(submitInfo, asFence);
    queue.waitIdle();

    // 9. scratch buffer can be destroyed after queue finishes
    utils::destroy_buffer(allocator, scratchBuffer);
    utils::destroy_buffer(allocator, instancesBuffer);

    return AccelerationStructure{.AS = tlas, .buffer = tlasBuffer};
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
    queueInfo.setQueueIndex(0); // Why I cannot use index 1?
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
