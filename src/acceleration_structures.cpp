#include "acceleration_structures.hpp"
#include "utils.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <set>

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

void ASBuilder::destroy()
{
    device.freeCommandBuffers(asPool, asCmd);
    device.destroyFence(asFence);
    device.destroyCommandPool(asPool);
    if (blasQueue.size() > 0) {
        for (auto &b : blasQueue) {
            device.destroyAccelerationStructureKHR(b.AS);
            utils::destroy_buffer(allocator, b.buffer);
        }
    }
}

AccelerationStructure ASBuilder::buildBLAS(const std::shared_ptr<MeshNode> &meshNode)
{
    VK_CHECK_RES(device.waitForFences(asFence, vk::True, FENCE_TIMEOUT));
    device.resetFences(asFence);

    // Geometry description (single triangle array)
    // const uint32_t numIndices = meshNode->mesh->indexBuffer->allocationInfo.size / sizeof(uint32_t);
    const uint32_t numVertices = meshNode->mesh->vertexBuffer->allocationInfo.size / sizeof(Vertex);

    const size_t numSurfaces = meshNode->mesh->surfaces.size();
    std::vector<vk::AccelerationStructureGeometryKHR> geometries(numSurfaces);
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges(numSurfaces);
    std::vector<uint32_t> primitiveCounts(numSurfaces);
    for (size_t i = 0; i < numSurfaces; i++) {
        const Surface &s = meshNode->mesh->surfaces[i];

        vk::AccelerationStructureGeometryTrianglesDataKHR triData{};
        triData.setVertexFormat(vk::Format::eR32G32B32Sfloat);
        triData.setVertexData(
            vk::DeviceOrHostAddressConstKHR{meshNode->mesh->vertexBuffer->bufferAddress});
        triData.setVertexStride(sizeof(Vertex));
        triData.setMaxVertex(numVertices - 1);
        triData.setIndexType(vk::IndexType::eUint32);
        triData.setIndexData(
            vk::DeviceOrHostAddressConstKHR{meshNode->mesh->indexBuffer->bufferAddress});

        vk::AccelerationStructureGeometryKHR geom{};
        geom.setGeometryType(vk::GeometryTypeKHR::eTriangles);
        geom.setFlags(vk::GeometryFlagBitsKHR::eOpaque); // simplest
        geom.setGeometry(triData);

        // The entire array will be used to build the BLAS.
        vk::AccelerationStructureBuildRangeInfoKHR offsets{};
        offsets.setFirstVertex(0);
        offsets.setPrimitiveCount(s.count / 3);
        offsets.setPrimitiveOffset(s.startIndex * sizeof(uint32_t));
        offsets.setTransformOffset(0);

        geometries[i] = geom;
        buildRanges[i] = offsets;
        primitiveCounts[i] = s.count / 3;
    }

    // Info necessary to get the vk::AccelerationStructureBuildSizesInfoKHR struct
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setMode(vk::BuildAccelerationStructureModeKHR::eBuild);
    buildInfo.setType(vk::AccelerationStructureTypeKHR::eBottomLevel);
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace);
    buildInfo.setGeometries(geometries);

    vk::AccelerationStructureBuildSizesInfoKHR buildSizes
        = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                       buildInfo,
                                                       primitiveCounts);

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
    // vk::AccelerationStructureBuildRangeInfoKHR buildRange{};
    // buildRange.setFirstVertex(0);
    // buildRange.setPrimitiveCount(numIndices / 3);
    // buildRange.setPrimitiveOffset(0);
    // buildRange.setFirstVertex(0);

    // Record and submit the command buffer
    utils::cmd_submit(device, queue, asFence, asCmd, [&](const vk::CommandBuffer &cmd) {
        cmd.buildAccelerationStructuresKHR(buildInfo, buildRanges.data());
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

    blasQueue.push_back(blas);
    return blas;
}

TopLevelAS ASBuilder::buildTLAS(const std::shared_ptr<GLTFObj> &scene)
{
    std::set<vk::DeviceAddress> indexBufferAddresses;
    std::unordered_map<vk::DeviceAddress, AccelerationStructure> uniqueBlases;
    std::vector<std::pair<AccelerationStructure, glm::mat4>> blases;
    blases.reserve(scene->surfaceCount);
    for (const auto &mn : scene->meshNodes) {
        const vk::DeviceAddress indexBufferAddress = mn->mesh->indexBuffer->bufferAddress;
        // Build BLAS for every unique mesh buffer
        if (indexBufferAddresses.count(indexBufferAddress) == 0)
            uniqueBlases[indexBufferAddress] = buildBLAS(mn);
        indexBufferAddresses.insert(indexBufferAddress);
        // Repeat blases each with its own transform matrix
        blases.emplace_back(std::make_pair(uniqueBlases[indexBufferAddress], mn->worldTransform));
    }

    // Here starts the vulkan stuff for building the tlas
    VK_CHECK_RES(device.waitForFences(asFence, vk::True, FENCE_TIMEOUT));
    device.resetFences(asFence);

    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    instances.reserve(blases.size());
    uint32_t instanceIndex = 0;
    for (const auto &b : blases) {
        const glm::mat3x4 transformGlm = glm::mat3x4(glm::transpose(b.second));
        const AccelerationStructure blas = b.first;
        vk::AccelerationStructureInstanceKHR instance{};
        vk::TransformMatrixKHR transformVk;
        memcpy(&transformVk.matrix, glm::value_ptr(transformGlm), sizeof(transformVk.matrix));
        instance.setTransform(transformVk);
        instance.setInstanceCustomIndex(instanceIndex); // gl_InstanceCustomIndexEXT
        instanceIndex++;
        instance.setAccelerationStructureReference(blas.addr);
        // instance.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque);
        instance.setMask(0xFF); //  Only be hit if rayMask & instance.mask != 0
        instance.setInstanceShaderBindingTableRecordOffset(
            0); // We will use the same hit group for all objects
        instances.emplace_back(instance);
    }

    // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
    vk::DeviceSize instancesSize = static_cast<vk::DeviceSize>(
        sizeof(vk::AccelerationStructureInstanceKHR) * instances.size());
    // Buffer of instances containing the matrices and BLAS ids
    Buffer instancesBuffer
        = utils::create_buffer(device,
                               allocator,
                               instancesSize,
                               vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
                                   | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                   | vk::BufferUsageFlagBits::eTransferDst,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    // Fill the buffer
    utils::copy_to_device_buffer(instancesBuffer,
                                 device,
                                 allocator,
                                 asCmd,
                                 queue,
                                 asFence,
                                 instances.data(),
                                 instancesSize);

    // Wraps a device pointer to the above uploaded instances.
    vk::AccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.setData(vk::DeviceOrHostAddressConstKHR{instancesBuffer.bufferAddress});

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    vk::AccelerationStructureGeometryKHR topASGeometry{};
    topASGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
    topASGeometry.setGeometry(instancesData);

    // Find sizes
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
                       | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate);
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
                                       VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

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
    // asCmd.reset();
    utils::cmd_submit(device, queue, asFence, asCmd, [&](const vk::CommandBuffer &cmd) {
        cmd.buildAccelerationStructuresKHR(buildInfo, &buildRangeInfo);
    });

    // Scratch buffer can be destroyed after queue finishes
    utils::destroy_buffer(allocator, scratchBuffer);
    // utils::destroy_buffer(allocator, instancesBuffer);

    return TopLevelAS{.as = tlas, .instances = instances, .instancesBuffer = instancesBuffer};
}

void ASBuilder::updateTLAS(TopLevelAS &tlas, const glm::mat4 &transform)
{
    // Here starts the vulkan stuff for building the tlas
    VK_CHECK_RES(device.waitForFences(asFence, vk::True, FENCE_TIMEOUT));
    device.resetFences(asFence);

    for (auto &i : tlas.instances) {
        glm::mat4 currentTransform{1.f};
        // VkTransformMatrixKHR is row-major 3x4, so we need to reconstruct it properly
        for (size_t row = 0; row < 3; ++row) {
            for (size_t col = 0; col < 4; ++col) {
                currentTransform[col][row] = i.transform.matrix[row][col];
            }
        }
        // Apply the new transform
        glm::mat4 newTransform = transform * currentTransform;
        // const glm::mat4x3 origTransform = glm::make_mat4x3((float *) i.transform.matrix.data());
        // const glm::mat3x4 transform0 = glm::transpose(glm::make_mat4x3(&i.transform.matrix[0][0]));
        // const glm::mat4 transform4x4 = transform * glm::mat4(origTransform);
        // const glm::mat3x4 transform3x4 = glm::mat3x4(glm::transpose(transform4x4));
        // const glm::mat3x4 transform3x4 = glm::mat3x4(glm::transpose(transform4x4));
        // Convert back to VkTransformMatrixKHR format (row-major 3x4)
        vk::TransformMatrixKHR transformVk;
        for (size_t row = 0; row < 3; ++row) {
            for (size_t col = 0; col < 4; ++col) {
                transformVk.matrix[row][col] = newTransform[col][row];
            }
        }
        // memcpy(&transformVk.matrix, glm::value_ptr(transform3x4), sizeof(transformVk.matrix));
        i.setTransform(transformVk);
    }

    // Fill the buffer
    const vk::DeviceSize instancesSize = static_cast<vk::DeviceSize>(
        sizeof(vk::AccelerationStructureInstanceKHR) * tlas.instances.size());
    utils::copy_to_device_buffer(tlas.instancesBuffer,
                                 device,
                                 allocator,
                                 asCmd,
                                 queue,
                                 asFence,
                                 tlas.instances.data(),
                                 instancesSize);

    // Wraps a device pointer to the above uploaded instances.
    vk::AccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.setData(vk::DeviceOrHostAddressConstKHR{tlas.instancesBuffer.bufferAddress});

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    vk::AccelerationStructureGeometryKHR topASGeometry{};
    topASGeometry.setGeometryType(vk::GeometryTypeKHR::eInstances);
    topASGeometry.setGeometry(instancesData);

    // Find sizes
    vk::AccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.setFlags(vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace
                       | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate);
    buildInfo.setGeometries(topASGeometry);
    buildInfo.setMode(vk::BuildAccelerationStructureModeKHR::eUpdate);
    buildInfo.setType(vk::AccelerationStructureTypeKHR::eTopLevel);
    buildInfo.setSrcAccelerationStructure(tlas.as.AS);
    buildInfo.setDstAccelerationStructure(tlas.as.AS);

    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo
        = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice,
                                                       buildInfo,
                                                       tlas.instances.size());

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
    buildInfo.setScratchData(scratchBuffer.bufferAddress);

    // Build Offsets info: n instances
    vk::AccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.setPrimitiveCount(tlas.instances.size());

    // Build the TLAS
    // Record the next set of commands
    // asCmd.reset();
    utils::cmd_submit(device, queue, asFence, asCmd, [&](const vk::CommandBuffer &cmd) {
        cmd.buildAccelerationStructuresKHR(buildInfo, &buildRangeInfo);
    });

    // Scratch buffer can be destroyed after queue finishes
    utils::destroy_buffer(allocator, scratchBuffer);
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
