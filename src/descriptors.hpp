#pragma once
#ifndef USE_CXX20_MODULES
#include <vulkan/vulkan.hpp>
#else
import vulkan;
#endif
#include "types.hpp"

struct Binding
{
    vk::DescriptorType type;
    vk::ShaderStageFlags shaderStageFlags;
    uint32_t binding;
    uint32_t descriptorCount{1};
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
    void add_storage(const vk::DescriptorSet &descSet,
                     const uint32_t binding,
                     const std::vector<Buffer> &storageBuffers);
    void add_as(const vk::DescriptorSet &descSet,
                const uint32_t binding,
                const vk::AccelerationStructureKHR &as);
    void add_storage_image(const vk::DescriptorSet &descSet,
                           const uint32_t binding,
                           const std::vector<ImageData> &images);
    void add_sampled_image(const vk::DescriptorSet &descSet,
                           const uint32_t binding,
                           const std::vector<ImageData> &images);
    void add_combined_image(const vk::DescriptorSet &descSet,
                            const uint32_t binding,
                            const std::vector<ImageData> &images);
    void add_sampler(const vk::DescriptorSet &descSet,
                     const uint32_t binding,
                     const std::vector<vk::Sampler> &samplers);
    void update();
    void clean();

private:
    const vk::Device &device;

    // We have to save all this Infos because they are accessed as an address in vk::WriteDescriptorSet,
    // and therefore we must avoid their unallocation. vk::WriteDescriptorSet is the structure passed to device.updateDescriptorSets()
    std::vector<std::tuple<vk::DescriptorSet, uint32_t, std::vector<vk::DescriptorBufferInfo>>>
        uniformInfos{};
    std::vector<std::tuple<vk::DescriptorSet, uint32_t, std::vector<vk::DescriptorBufferInfo>>>
        storageInfos{};
    std::vector<std::tuple<vk::DescriptorSet, uint32_t, std::vector<vk::DescriptorImageInfo>>>
        imageStorageInfos{};
    std::vector<std::tuple<vk::DescriptorSet, uint32_t, std::vector<vk::DescriptorImageInfo>>>
        imageSampledInfos{};
    std::vector<std::tuple<vk::DescriptorSet, uint32_t, std::vector<vk::DescriptorImageInfo>>>
        combinedImageInfos{};
    std::vector<std::tuple<vk::DescriptorSet, uint32_t, std::vector<vk::DescriptorImageInfo>>>
        samplerInfos{};
    std::vector<
        std::tuple<vk::DescriptorSet, uint32_t, vk::WriteDescriptorSetAccelerationStructureKHR>>
        tlasWritesKHR{};
    size_t descriptorCount{0};
};
