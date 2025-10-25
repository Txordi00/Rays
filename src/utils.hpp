#pragma once

#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "types.hpp"
#include <functional>
#include <glm/glm.hpp>

namespace utils {

void transition_image(
    const vk::CommandBuffer &cmd,
    const ImageData &image,
    const vk::ImageLayout &currentLayout,
    const vk::ImageLayout &newLayout,
    const vk::PipelineStageFlags2 &srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
    const vk::PipelineStageFlags2 &dstStageMask = vk::PipelineStageFlagBits2::eAllCommands);

void copy_image(const vk::CommandBuffer &cmd,
                const vk::Image &src,
                const vk::Image &dst,
                const vk::Extent2D &srcRes,
                const vk::Extent2D &dstRes);

Buffer create_buffer(const vk::Device &device,
                     const VmaAllocator &allocator,
                     const vk::DeviceSize &size,
                     const vk::BufferUsageFlags &usageFlags,
                     const VmaMemoryUsage &memoryUsage,
                     const VmaAllocationCreateFlags &allocationFlags = 0,
                     const vk::DeviceSize alignment = 0);

ImageData create_image(const vk::Device &device,
                       const VmaAllocator &allocator,
                       const vk::CommandBuffer &cmd,
                       const vk::Fence &fence,
                       const vk::Queue &queue,
                       const vk::Format &format,
                       const vk::ImageUsageFlags &flags,
                       const vk::Extent3D &extent,
                       const void *data = nullptr);

void copy_to_image(const vk::Device &device,
                   const VmaAllocator &allocator,
                   const vk::CommandBuffer &cmd,
                   const vk::Fence &fence,
                   const vk::Queue &queue,
                   const vk::Image &image,
                   const vk::Extent3D &extent,
                   const void *data);

void destroy_buffer(const VmaAllocator &allocator, const Buffer &buffer);

void copy_to_buffer(const Buffer &buffer,
                   const VmaAllocator &allocator,
                   const void *data,
                   const vk::DeviceSize size = vk::WholeSize,
                   const vk::DeviceSize offset = 0);

vk::ShaderModule load_shader(const vk::Device &device, const std::string filePath);

glm::mat4 get_perspective_projection(const float fovy,
                                     const float aspect,
                                     const float near,
                                     const float far);

uint32_t align_up(uint32_t x, uint32_t a);

void normalize_material_factors(Material &material);

void cmd_submit(const vk::Device &device,
                const vk::Queue &queue,
                const vk::Fence &fence,
                const vk::CommandBuffer &cmd,
                std::function<void(const vk::CommandBuffer &cmd)> &&function);

namespace init {
vk::ImageCreateInfo image_create_info(const vk::Format &format,
                                      const vk::ImageUsageFlags &flags,
                                      const vk::Extent3D &extent);

vk::ImageViewCreateInfo image_view_create_info(const vk::Format &format,
                                               const vk::Image &image,
                                               const vk::ImageAspectFlags &aspectMask);

} // namespace init

} // namespace utils
