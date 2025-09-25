#pragma once

#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include "types.hpp"
#include <glm/glm.hpp>

namespace utils {

void transition_image(
    vk::CommandBuffer &cmd,
    const vk::Image &image,
    const vk::ImageLayout &currentLayout,
    const vk::ImageLayout &newLayout,
    const vk::PipelineStageFlags2 &srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
    const vk::PipelineStageFlags2 &dstStageMask = vk::PipelineStageFlagBits2::eAllCommands);

void copy_image(vk::CommandBuffer &cmd,
                const vk::Image &src,
                const vk::Image &dst,
                const vk::Extent2D &srcRes,
                const vk::Extent2D &dstRes);

Buffer create_buffer(const vk::Device &device,
                     const VmaAllocator &allocator,
                     const vk::DeviceSize &size,
                     const vk::BufferUsageFlags &usageFlags,
                     const VmaMemoryUsage &memoryUsage,
                     const VmaAllocationCreateFlags &allocationFlags,
                     const vk::DeviceSize alignment = 0);

void destroy_buffer(const VmaAllocator &allocator, const Buffer &buffer);

void map_to_buffer(const Buffer &buffer, const void *data);

vk::ShaderModule load_shader(const vk::Device &device, const std::string filePath);

glm::mat4 get_perspective_projection(const float fovy,
                                     const float aspect,
                                     const float near,
                                     const float far);

uint32_t align_up(uint32_t x, uint32_t a);

void normalize_material_factors(Material &material);

namespace init {
vk::ImageCreateInfo image_create_info(const vk::Format &format,
                                      const vk::ImageUsageFlags &flags,
                                      const vk::Extent3D &extent);

vk::ImageViewCreateInfo image_view_create_info(const vk::Format &format,
                                               const vk::Image &image,
                                               const vk::ImageAspectFlags &aspectMask);

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
