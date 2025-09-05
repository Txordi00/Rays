#include "utils.hpp"
#include <fstream>
#include <iostream>

namespace utils {
void transition_image(vk::CommandBuffer &cmd,
                      const vk::Image &image,
                      const vk::ImageLayout &currentLayout,
                      const vk::ImageLayout &newLayout)
{
    vk::ImageMemoryBarrier2 imageBarrier{};
    // The AllCommands flags can be optimized
    imageBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eAllCommands);
    imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eMemoryWrite);
    imageBarrier.setDstStageMask(vk::PipelineStageFlagBits2::eAllCommands);
    imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eMemoryWrite
                                  | vk::AccessFlagBits2::eMemoryRead);

    imageBarrier.setOldLayout(currentLayout);
    imageBarrier.setNewLayout(newLayout);

    vk::ImageAspectFlags aspectMask = (newLayout == vk::ImageLayout::eDepthAttachmentOptimal)
                                          ? vk::ImageAspectFlagBits::eDepth
                                          : vk::ImageAspectFlagBits::eColor;

    vk::ImageSubresourceRange imSubresrcRange{};
    imSubresrcRange.setAspectMask(aspectMask);
    imSubresrcRange.setBaseMipLevel(0);
    imSubresrcRange.setLevelCount(vk::RemainingMipLevels);
    imSubresrcRange.setBaseArrayLayer(0);
    imSubresrcRange.setLayerCount(vk::RemainingArrayLayers);
    imageBarrier.setSubresourceRange(imSubresrcRange);

    imageBarrier.setImage(image);

    vk::DependencyInfo depInfo{};
    depInfo.setImageMemoryBarrierCount(1);
    depInfo.setPImageMemoryBarriers(&imageBarrier);

    cmd.pipelineBarrier2(depInfo);
}

void copy_image(vk::CommandBuffer &cmd,
                const vk::Image &src,
                const vk::Image &dst,
                const vk::Extent2D &srcRes,
                const vk::Extent2D &dstRes)
{
    vk::ImageBlit2 blitRegion{};
    vk::Offset3D startOffset{0, 0, 0};
    vk::Offset3D endSrcOffset{static_cast<int32_t>(srcRes.width),
                              static_cast<int32_t>(srcRes.height),
                              1};
    vk::Offset3D endDstOffset{static_cast<int32_t>(dstRes.width),
                              static_cast<int32_t>(dstRes.height),
                              1};
    blitRegion.setSrcOffsets({startOffset, endSrcOffset});
    blitRegion.setDstOffsets({startOffset, endDstOffset});

    blitRegion.setSrcSubresource(
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1});

    blitRegion.setDstSubresource(
        vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1});

    vk::BlitImageInfo2 blitInfo{};
    blitInfo.setDstImage(dst);
    blitInfo.setDstImageLayout(vk::ImageLayout::eTransferDstOptimal);
    blitInfo.setSrcImage(src);
    blitInfo.setSrcImageLayout(vk::ImageLayout::eTransferSrcOptimal);
    blitInfo.setRegions(blitRegion);
    blitInfo.setFilter(vk::Filter::eLinear);

    cmd.blitImage2(blitInfo);
}

vk::ShaderModule load_shader(const vk::Device &device, const std::string filePath)
{
    // open the file. With cursor at the end
    std::ifstream file(filePath, std::ifstream::ate | std::ifstream::binary);

    if (!file.is_open())
        throw std::runtime_error("Shader binary " + filePath + " could not be opened");

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t fileSize = static_cast<size_t>(file.tellg());

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));

    // put file cursor at beginning
    file.seekg(0);

    // load the entire file into the buffer
    file.read((char *) code.data(), fileSize);

    // now that the file is loaded into the buffer, we can close it
    file.close();

    // Create the shader module in the device and return it
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.setCode(code);

    return device.createShaderModule(shaderModuleCreateInfo);
}

glm::mat4 get_perspective_projection(const float fovy,
                                     const float aspect,
                                     const float near,
                                     const float far)
{
    assert(std::abs(aspect) > std::numeric_limits<float>::epsilon());
    const float tanHalfFovy = tan(fovy / 2.f);
    glm::mat4 projectionMatrix{0.0f};
    projectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
    projectionMatrix[1][1] = 1.f / (tanHalfFovy);
    projectionMatrix[2][2] = far / (far - near);
    projectionMatrix[2][3] = 1.f;
    projectionMatrix[3][2] = -(far * near) / (far - near);

    return projectionMatrix;
}

Buffer create_buffer(const vk::Device &device,
                     const VmaAllocator &allocator,
                     const vk::DeviceSize &size,
                     const vk::BufferUsageFlags &usageFlags,
                     const VmaMemoryUsage &memoryUsage,
                     const VmaAllocationCreateFlags &allocationFlags,
                     const vk::DeviceSize alignment)
{
    vk::BufferCreateInfo bufferInfo{};
    bufferInfo.setUsage(usageFlags);
    bufferInfo.setSize(size);

    VmaAllocationCreateInfo vmaallocInfo{};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = allocationFlags;

    Buffer createdBuffer;
    if (alignment == 0) {
        VK_CHECK_RES(vmaCreateBuffer(allocator,
                                     (VkBufferCreateInfo *) &bufferInfo,
                                     &vmaallocInfo,
                                     (VkBuffer *) &createdBuffer.buffer,
                                     &createdBuffer.allocation,
                                     &createdBuffer.allocationInfo));
    } else {
        vmaCreateBufferWithAlignment(allocator,
                                     (VkBufferCreateInfo *) &bufferInfo,
                                     &vmaallocInfo,
                                     alignment,
                                     (VkBuffer *) &createdBuffer.buffer,
                                     &createdBuffer.allocation,
                                     &createdBuffer.allocationInfo);
    }

    if (usageFlags & vk::BufferUsageFlagBits::eShaderDeviceAddress) {
        vk::BufferDeviceAddressInfo addressInfo{};
        addressInfo.setBuffer(createdBuffer.buffer);
        createdBuffer.bufferAddress = device.getBufferAddress(addressInfo);
    }

    return createdBuffer;
}

void destroy_buffer(const VmaAllocator &allocator, const Buffer &buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

uint32_t align_up(uint32_t x, uint32_t a)
{
    return uint32_t((x + (uint32_t(a) - 1)) & ~uint32_t(a - 1));
}

namespace init {
vk::ImageCreateInfo image_create_info(const vk::Format &format,
                                      const vk::ImageUsageFlags &flags,
                                      const vk::Extent3D &extent)
{
    vk::ImageCreateInfo imageCreateInfo{};
    imageCreateInfo.setImageType(vk::ImageType::e2D);
    imageCreateInfo.setFormat(format);
    imageCreateInfo.setExtent(extent);
    imageCreateInfo.setUsage(flags);
    imageCreateInfo.setMipLevels(1);
    imageCreateInfo.setArrayLayers(1);
    imageCreateInfo.setSamples(vk::SampleCountFlagBits::e1);
    imageCreateInfo.setTiling(vk::ImageTiling::eOptimal);
    // ENABLE when the driver will support VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION
    // imageCreateInfo.setInitialLayout(vk::ImageLayout::eGeneral);
    return imageCreateInfo;
}

vk::ImageViewCreateInfo image_view_create_info(const vk::Format &format,
                                               const vk::Image &image,
                                               const vk::ImageAspectFlags &aspectMask)
{
    vk::ImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.setImage(image);
    imageViewCreateInfo.setFormat(format);
    imageViewCreateInfo.setViewType(vk::ImageViewType::e2D);
    vk::ImageSubresourceRange imSubResRan{};
    imSubResRan.setAspectMask(aspectMask);
    imSubResRan.setBaseMipLevel(0);
    imSubResRan.setLevelCount(1);
    imSubResRan.setBaseArrayLayer(0);
    imSubResRan.setLayerCount(1);
    imageViewCreateInfo.setSubresourceRange(imSubResRan);

    return imageViewCreateInfo;
}

} // namespace init

void map_to_buffer(const Buffer &buffer, const void *data)
{
    assert(buffer.allocationInfo.pMappedData && "Buffer must be mapped!");
    memcpy(buffer.allocationInfo.pMappedData, data, buffer.allocationInfo.size);
}

// namespace init

} // namespace utils
