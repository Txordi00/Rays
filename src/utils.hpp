#pragma once

#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

namespace utils {
void transition_image(vk::CommandBuffer &cmd,
                      vk::Image &image,
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

} // namespace utils
