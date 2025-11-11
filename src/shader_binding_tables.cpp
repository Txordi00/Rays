#include "shader_binding_tables.hpp"
#include "utils.hpp"
#include <iostream>

//--------------------------------------------------------------------------------------------------
// The Shader Binding Table (SBT)
// - getting all shader handles and write them in a SBT buffer
// - Besides exception, this could be always done like this
//
Buffer SbtHelper::create_shader_binding_table(const vk::Pipeline &rtPipeline)
{
    uint32_t missCount{2};
    uint32_t hitCount{1};
    uint32_t handleCount = 1 + missCount + hitCount;
    uint32_t handleSize = rtProperties.shaderGroupHandleSize;

    // The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
    uint32_t handleSizeAligned = utils::align_up(handleSize,
                                                 rtProperties.shaderGroupHandleAlignment);

    rgenRegion.setStride(utils::align_up(handleSizeAligned, rtProperties.shaderGroupBaseAlignment));
    // The size member of pRayGenShaderBindingTable must be equal to its stride member
    rgenRegion.setSize(rgenRegion.stride);

    missRegion.setStride(handleSizeAligned);
    missRegion.setSize(
        utils::align_up(missCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment));

    hitRegion.setStride(handleSizeAligned);
    hitRegion.setSize(
        utils::align_up(hitCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment));

    // Get the shader group handles
    uint32_t dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles = device.getRayTracingShaderGroupHandlesKHR<uint8_t>(rtPipeline,
                                                                                      0,
                                                                                      handleCount,
                                                                                      dataSize);

    // Allocate a buffer for storing the SBT.
    vk::DeviceSize sbtSize = rgenRegion.size + missRegion.size + hitRegion.size;
    Buffer rtSBTBuffer = utils::create_buffer(device,
                                              allocator,
                                              sbtSize,
                                              vk::BufferUsageFlagBits::eTransferSrc
                                                  | vk::BufferUsageFlagBits::eShaderDeviceAddress
                                                  | vk::BufferUsageFlagBits::eShaderBindingTableKHR,
                                              VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                              VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                  | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // Find the SBT addresses of each group
    vk::BufferDeviceAddressInfo deviceAdressInfo{};
    deviceAdressInfo.setBuffer(rtSBTBuffer.buffer);
    vk::DeviceAddress sbtAddress = device.getBufferAddress(deviceAdressInfo);
    rgenRegion.setDeviceAddress(sbtAddress);
    missRegion.setDeviceAddress(sbtAddress + rgenRegion.size);
    hitRegion.setDeviceAddress(sbtAddress + rgenRegion.size + missRegion.size);

    // Helper to retrieve the handle data
    auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

    // Map the SBT buffer and write in the handles.
    uint8_t *pBuffer = (uint8_t *) rtSBTBuffer.allocationInfo.pMappedData;
    uint8_t *pData = pBuffer;
    uint32_t handleIdx = 0, a = 0;
    // Raygen
    memcpy(pData, getHandle(handleIdx++), handleSize);
    // Miss
    pData = pBuffer + rgenRegion.size;
    for (uint32_t c = 0; c < missCount; c++) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += missRegion.stride;
    }
    // Hit
    pData = pBuffer + rgenRegion.size + missRegion.size;
    for (uint32_t c = 0; c < hitCount; c++) {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += hitRegion.stride;
    }

    return rtSBTBuffer;
}
