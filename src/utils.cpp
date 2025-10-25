#include "utils.hpp"
#include <fstream>
#include <functional>
#include <iostream>

namespace utils {
void transition_image(const vk::CommandBuffer &cmd,
                      const ImageData &image,
                      const vk::ImageLayout &currentLayout,
                      const vk::ImageLayout &newLayout,
                      const vk::PipelineStageFlags2 &srcStageMask,
                      const vk::PipelineStageFlags2 &dstStageMask)
{
    vk::ImageMemoryBarrier2 imageBarrier{};
    // The AllCommands flags can be optimized
    imageBarrier.setSrcStageMask(srcStageMask);
    imageBarrier.setSrcAccessMask(vk::AccessFlagBits2::eMemoryWrite);
    imageBarrier.setDstStageMask(dstStageMask);
    imageBarrier.setDstAccessMask(vk::AccessFlagBits2::eMemoryWrite
                                  | vk::AccessFlagBits2::eMemoryRead);

    imageBarrier.setOldLayout(currentLayout);
    imageBarrier.setNewLayout(newLayout);

    vk::ImageAspectFlags aspectMask = (image.format == vk::Format::eD32Sfloat)
                                          ? vk::ImageAspectFlagBits::eDepth
                                          : vk::ImageAspectFlagBits::eColor;

    vk::ImageSubresourceRange imSubresrcRange{};
    imSubresrcRange.setAspectMask(aspectMask);
    imSubresrcRange.setBaseMipLevel(0);
    imSubresrcRange.setLevelCount(vk::RemainingMipLevels);
    imSubresrcRange.setBaseArrayLayer(0);
    imSubresrcRange.setLayerCount(vk::RemainingArrayLayers);
    imageBarrier.setSubresourceRange(imSubresrcRange);

    imageBarrier.setImage(image.image);

    vk::DependencyInfo depInfo{};
    depInfo.setImageMemoryBarriers(imageBarrier);

    cmd.pipelineBarrier2(depInfo);
}

void copy_image(const vk::CommandBuffer &cmd,
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
    blitInfo.setDstImageLayout(vk::ImageLayout::eGeneral);
    blitInfo.setSrcImage(src);
    blitInfo.setSrcImageLayout(vk::ImageLayout::eGeneral);
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

    if ((usageFlags & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        == vk::BufferUsageFlagBits::eShaderDeviceAddress) {
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

void copy_to_buffer(const Buffer &buffer,
                    const VmaAllocator &allocator,
                    const void *data,
                    const vk::DeviceSize size,
                    const vk::DeviceSize offset)
{
    const vk::DeviceSize s = (size == vk::WholeSize) ? buffer.allocationInfo.size : size;
    vmaCopyMemoryToAllocation(allocator, data, buffer.allocation, offset, s);
}

void normalize_material_factors(Material &m)
{
    float normFactor = m.specularR + m.diffuseR + m.ambientR + m.reflectiveness + m.refractiveness;
    m.specularR /= normFactor;
    m.diffuseR /= normFactor;
    m.ambientR /= normFactor;
    m.reflectiveness /= normFactor;
    m.refractiveness /= normFactor;
}

ImageData create_image(const vk::Device &device,
                       const VmaAllocator &allocator,
                       const vk::CommandBuffer &cmd,
                       const vk::Fence &fence,
                       const vk::Queue &queue,
                       const vk::Format &format,
                       const vk::ImageUsageFlags &flags,
                       const vk::Extent3D &extent,
                       const void *data)
{
    ImageData image;

    // Overkill format
    image.format = format;
    image.extent = extent;

    const vk::ImageUsageFlags usageFlags = (data) ? flags | vk::ImageUsageFlagBits::eTransferDst
                                                  : flags;

    const vk::ImageCreateInfo imageCreateInfo = utils::init::image_create_info(format,
                                                                               usageFlags,
                                                                               extent);

    VmaAllocationCreateInfo allocationCreateInfo{};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    // Efficiently create and allocate the image with VMA
    vmaCreateImage(allocator,
                   (VkImageCreateInfo *) &imageCreateInfo,
                   &allocationCreateInfo,
                   (VkImage *) &image.image,
                   &image.allocation,
                   &image.allocationInfo);

    // Only 2 options: Color or depth. Decide depending on the format
    const vk::ImageAspectFlags aspectFlags = (format == vk::Format::eD32Sfloat)
                                                 ? vk::ImageAspectFlagBits::eDepth
                                                 : vk::ImageAspectFlagBits::eColor;

    // Create the handle vk::ImageView. Not possible to do this with VMA
    const vk::ImageViewCreateInfo imageViewCreateInfo
        = utils::init::image_view_create_info(format, image.image, aspectFlags);
    image.imageView = device.createImageView(imageViewCreateInfo);

    // Thanks to the extension UINIFIED_IMAGE_LAYOUTS, we can safely move to
    // the layout General and forget about layout transitions without paying
    // any performance tax. Stage flags set to none since we already wait for fences.
    utils::cmd_submit(device, queue, fence, cmd, [&](const vk::CommandBuffer &cmd) {
        utils::transition_image(cmd,
                                image,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eGeneral,
                                vk::PipelineStageFlagBits2::eNone,
                                vk::PipelineStageFlagBits2::eNone);
    });

    // Map data if requested so
    if (data != nullptr)
        utils::copy_to_image(device, allocator, cmd, fence, queue, image.image, extent, data);

    return image;
}

void copy_to_image(const vk::Device &device,
                   const VmaAllocator &allocator,
                   const vk::CommandBuffer &cmd,
                   const vk::Fence &fence,
                   const vk::Queue &queue,
                   const vk::Image &image,
                   const vk::Extent3D &extent,
                   const void *data)
{
    // We need to move the data into the image
    assert(data != nullptr && "Data to map must be non-null");
    vk::DeviceSize dataSize = extent.width * extent.height * extent.depth * 4;
    Buffer tmpBuffer = create_buffer(device,
                                     allocator,
                                     dataSize,
                                     vk::BufferUsageFlagBits::eTransferSrc,
                                     VMA_MEMORY_USAGE_AUTO,
                                     VMA_ALLOCATION_CREATE_MAPPED_BIT
                                         | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    utils::copy_to_buffer(tmpBuffer, allocator, data);

    utils::cmd_submit(device, queue, fence, cmd, [&](const vk::CommandBuffer &cmd) {
        vk::ImageSubresourceLayers subResource{};
        subResource.setAspectMask(vk::ImageAspectFlagBits::eColor);
        subResource.setLayerCount(1);
        vk::BufferImageCopy2 copyRegion{};
        copyRegion.setImageExtent(extent);
        copyRegion.setImageSubresource(subResource);
        vk::CopyBufferToImageInfo2 copyInfo{};
        copyInfo.setSrcBuffer(tmpBuffer.buffer);
        copyInfo.setDstImage(image);
        copyInfo.setDstImageLayout(vk::ImageLayout::eGeneral);
        copyInfo.setRegions(copyRegion);

        cmd.copyBufferToImage2(copyInfo);
    });
    utils::destroy_buffer(allocator, tmpBuffer);
}

void cmd_submit(const vk::Device &device,
                const vk::Queue &queue,
                const vk::Fence &fence,
                const vk::CommandBuffer &cmd,
                std::function<void(const vk::CommandBuffer &cmd)> &&function)
{
    // We should be good to go without waitForFences() and reset() here
    device.resetFences(fence);
    // cmd.reset();

    // begin the command buffer recording. We will use this command buffer exactly
    // once, so we want to let vulkan know that
    vk::CommandBufferBeginInfo cmdBegin{};
    cmdBegin.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(cmdBegin);

    // Function to record
    function(cmd);

    // Do not record anything else
    cmd.end();

    vk::CommandBufferSubmitInfo cmdInfo{};
    cmdInfo.setCommandBuffer(cmd);
    cmdInfo.setDeviceMask(1);
    vk::SubmitInfo2 submitInfo{};
    submitInfo.setCommandBufferInfos(cmdInfo);

    // submit command buffer to the queue and execute it.
    // Fence will block the host until the commands in cmd finish execution
    queue.submit2(submitInfo, fence);
    VK_CHECK_RES(device.waitForFences(fence, vk::True, FENCE_TIMEOUT));
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
    imageCreateInfo.setInitialLayout(vk::ImageLayout::eUndefined);
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

// namespace init

// namespace init

} // namespace utils
