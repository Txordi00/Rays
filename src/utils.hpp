#pragma once

// #include <deque>
// #include <functional>
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

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

// struct DeletionQueue
// {
//     std::deque<std::function<void()>> deletors;

//     void push_function(std::function<void()> &&function) { deletors.push_back(function); }

//     void flush()
//     {
//         // reverse iterate the deletion queue to execute all the functions
//         for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
//             (*it)(); //call functors
//         }

//         deletors.clear();
//     }
// };

} // namespace utils
