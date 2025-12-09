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
    const uint32_t numIndices = meshNode->mesh->indexBuffer->allocationInfo.size / sizeof(uint32_t);
    const uint32_t numVertices = meshNode->mesh->vertexBuffer->allocationInfo.size / sizeof(Vertex);
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
    // geom.setFlags(vk::GeometryFlagBitsKHR::eOpaque); // simplest
    geom.setGeometry(triData);

    // The entire array will be used to build the BLAS.
    vk::AccelerationStructureBuildRangeInfoKHR offsets{};
    offsets.setFirstVertex(0);
    offsets.setPrimitiveCount(numIndices / 3);
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

    blasQueue.push_back(blas);
    return blas;
}

AccelerationStructure ASBuilder::buildTLAS(const std::shared_ptr<GLTFObj> &scene)
{
    // Classify all MeshNodes by uniqueness of their device address
    std::unordered_multimap<vk::DeviceAddress, std::shared_ptr<MeshNode>> meshNodes;
    for (const auto &mn : scene->meshNodes)
        meshNodes.insert({mn->mesh->indexBuffer->bufferAddress, mn});

    // The uint32 key is going to be the instanceCustomIndex, blases are
    // going to be repeated: one per Mesh.
    std::vector<std::tuple<uint32_t, AccelerationStructure, glm::mat4>> blases;
    blases.reserve(scene->surfaceStorageBuffersCount);
    for (auto it = meshNodes.begin(); it != meshNodes.end();) {
        AccelerationStructure blas = buildBLAS(it->second);
        // Get all the MeshNodes associated with a single mesh
        const auto &[rangeFirst, rangeEnd] = meshNodes.equal_range(it->first);
        for (auto r = rangeFirst; r != rangeEnd; ++r) {
            for (const auto &[key, buffer] : r->second->surfaceUniformBuffers) {
                blases.push_back({key, blas, glm::transpose(r->second->worldTransform)});
            }
        }
        it = rangeEnd;
    }

    // Here starts the vulkan stuff for building the tlas
    VK_CHECK_RES(device.waitForFences(asFence, vk::True, FENCE_TIMEOUT));
    device.resetFences(asFence);

    std::vector<vk::AccelerationStructureInstanceKHR> instances;
    instances.reserve(blases.size());
    for (const auto &b : blases) {
        const glm::mat3x4 transformGlm = glm::mat3x4(std::get<2>(b));
        const AccelerationStructure blas = std::get<1>(b);
        const uint32_t customIndex = std::get<0>(b);
        vk::AccelerationStructureInstanceKHR instance{};
        vk::TransformMatrixKHR transformVk;
        memcpy(&transformVk, glm::value_ptr(transformGlm), sizeof(vk::TransformMatrixKHR));
        instance.setTransform(transformVk);
        instance.setInstanceCustomIndex(customIndex); // gl_InstanceCustomIndexEXT
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
                                   | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                               VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                               VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    // Fill the buffer
    utils::copy_to_buffer(instancesBuffer, allocator, instances.data());

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
    utils::destroy_buffer(allocator, instancesBuffer);

    return tlas;
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
