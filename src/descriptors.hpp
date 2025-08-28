#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif
#include "types.hpp"

struct Binding
{
    vk::DescriptorType type;
    vk::ShaderStageFlags shaderStageFlags;
    uint32_t binding;
};

class DescHelper
{
public:
    DescHelper(const vk::Device &device,
               const vk::PhysicalDeviceProperties &physDevProp,
               const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties,
               const bool updateAfterBind);
    ~DescHelper() = default;
    void destroy();
    void add_descriptor_set(const vk::DescriptorPoolSize &poolSize, const uint32_t numSets);
    void create_descriptor_pool();
    void add_binding(const Binding &bind);
    vk::DescriptorSetLayout create_descriptor_set_layout();
    std::vector<vk::DescriptorSet> allocate_descriptor_sets(
        const vk::DescriptorSetLayout &descriptorSetLayout, const uint32_t frameOverlap);

private:
    const vk::Device &device;
    const vk::PhysicalDeviceProperties &physDevProp;
    const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties;
    const bool updateAfterBind;

    vk::DescriptorPool pool;
    std::vector<vk::DescriptorPoolSize> poolSizes;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

class DescriptorUpdater
{
public:
    DescriptorUpdater(const vk::Device &device)
        : device{device}
    {}
    ~DescriptorUpdater() = default;
    void add_uniform(const vk::DescriptorSet &descSet,
                     const uint32_t binding,
                     const std::vector<Buffer> &uniformBuffers);
    void add_as(const vk::DescriptorSet &descSet,
                const uint32_t binding,
                const vk::AccelerationStructureKHR &as);
    void add_image(const vk::DescriptorSet &descSet,
                   const uint32_t binding,
                   const vk::ImageView &imageView);
    void update();

private:
    const vk::Device &device;
    std::vector<vk::WriteDescriptorSet> descriptorWrites{};

    // These are declared here in order to not be lost when they get out of scope
    std::vector<vk::DescriptorBufferInfo> bufferInfos{};
    vk::DescriptorImageInfo imageInfo{};
    vk::WriteDescriptorSetAccelerationStructureKHR tlasWriteKHR{};
};

void update_descriptor_sets(const vk::Device &device,
                            const std::optional<std::vector<Buffer> > &uniformBuffers = std::nullopt,
                            const std::optional<vk::DescriptorSet> &uniformSet = std::nullopt,
                            const std::optional<uint32_t> uBinding = std::nullopt,
                            const std::optional<vk::AccelerationStructureKHR> &tlas = std::nullopt,
                            const std::optional<vk::DescriptorSet> &tlasSet = std::nullopt,
                            const std::optional<uint32_t> asBinding = std::nullopt,
                            const std::optional<vk::ImageView> &imageView = std::nullopt,
                            const std::optional<vk::DescriptorSet> &imageSet = std::nullopt,
                            const std::optional<uint32_t> imBinding = std::nullopt);
