#include "shader_binding_tables.hpp"
#include "utils.hpp"
#include <print>

//--------------------------------------------------------------------------------------------------
// The Shader Binding Table (SBT)
// - getting all shader handles and write them in a SBT buffer
// - Besides exception, this could be always done like this
//
Buffer SbtHelper::create_shader_binding_table()
{
    uint32_t missCount{1};
    uint32_t hitCount{1};
    uint32_t handleCount = 1 + missCount + hitCount;
    uint32_t handleSize = rtProperties.shaderGroupHandleSize;

    // The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
    uint32_t handleSizeAligned = utils::align_up(handleSize,
                                                 rtProperties.shaderGroupHandleAlignment);

    vk::StridedDeviceAddressRegionKHR rgenRegion{};
    vk::StridedDeviceAddressRegionKHR missRegion{};
    vk::StridedDeviceAddressRegionKHR hitRegion{};

    rgenRegion.setStride(utils::align_up(handleSizeAligned, rtProperties.shaderGroupBaseAlignment));
    // The size member of pRayGenShaderBindingTable must be equal to its stride member
    rgenRegion.setSize(rgenRegion.stride);

    missRegion.setStride(handleSizeAligned);
    missRegion.setSize(
        utils::align_up(missCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment));

    hitRegion.setStride(handleSizeAligned);
    hitRegion.setSize(
        utils::align_up(hitCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment));

    std::println("a");
}
