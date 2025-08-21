#include "descriptors.hpp"

DescHelper::DescHelper(const vk::Device &device,
                       const vk::PhysicalDeviceProperties &physDevProp,
                       const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties,
                       const bool updateAfterBind)
    : device{device}
    , physDevProp{physDevProp}
    , asProperties{asProperties}
    , updateAfterBind{updateAfterBind}
{
}

void DescHelper::destroy()
{
    device.destroyDescriptorPool(pool);
}

void DescHelper::add_descriptor_set(const vk::DescriptorPoolSize &poolSize, const uint32_t numSets)
{
    uint32_t numUniformDescriptors = 0, numStorageImageDescriptors = 0, numASDescriptors = 0;
    poolSizes.reserve(numSets);
    for (int i = 0; i < numSets; i++) {
        poolSizes.emplace_back(poolSize);
    }
}

void DescHelper::create_descriptor_pool()
{
    vk::DescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.setMaxSets(poolSizes.size());
    poolCreateInfo.setPoolSizes(poolSizes);
    if (updateAfterBind)
        poolCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

    pool = device.createDescriptorPool(poolCreateInfo);
}

void DescHelper::add_binding(const vk::DescriptorType &type,
                             const vk::ShaderStageFlags &shaderStageFlags)
{
    uint32_t maxDescriptors = 0;
    for (const auto &ps : poolSizes)
        if (type == ps.type)
            maxDescriptors = std::max(maxDescriptors, ps.descriptorCount);

    vk::DescriptorSetLayoutBinding binding{};
    binding.setBinding(BINDING_DICT.at(type));
    binding.setDescriptorType(type);
    binding.setDescriptorCount(maxDescriptors);
    binding.setStageFlags(shaderStageFlags);

    bindings.push_back(binding);
}

// Returns a pair (bindless ds layout, rt ds layout)
vk::DescriptorSetLayout DescHelper::create_descriptor_set_layout()
{
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.setBindings(bindings);

    vk::DescriptorBindingFlags bindlessBindingFlags{
        vk::DescriptorBindingFlagBits::ePartiallyBound
        | vk::DescriptorBindingFlagBits::eUpdateAfterBind};
    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindlessBindingFlagsInfo{};
    bindlessBindingFlagsInfo.setBindingFlags(bindlessBindingFlags);
    if (updateAfterBind) {
        descriptorSetLayoutInfo.setFlags(
            vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
        descriptorSetLayoutInfo.setPNext(&bindlessBindingFlagsInfo);
    }

    return device.createDescriptorSetLayout(descriptorSetLayoutInfo);
}

std::vector<vk::DescriptorSet> DescHelper::allocate_descriptor_sets(
    const vk::DescriptorSetLayout &descriptorSetLayout, const uint32_t frameOverlap)
{
    std::vector<vk::DescriptorSetLayout> descriptorSetLayoutPerFrame(frameOverlap,
                                                                     descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(pool);
    allocInfo.setSetLayouts(descriptorSetLayoutPerFrame);

    return device.allocateDescriptorSets(allocInfo);
}

void update_descriptor_sets(const vk::Device &device,
                            const std::optional<std::vector<Buffer> > &uniformBuffers,
                            const std::optional<vk::DescriptorSet> &uniformSet,
                            const std::optional<vk::AccelerationStructureKHR> &tlas,
                            const std::optional<vk::DescriptorSet> &tlasSet,
                            const std::optional<vk::ImageView> &imageView,
                            const std::optional<vk::DescriptorSet> &imageSet)
{
    std::vector<vk::WriteDescriptorSet> descriptorWrites{};
    // Add all the buffers to a single descriptor write
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(uniformBuffers.value().size());
    if (uniformBuffers.has_value() && uniformSet.has_value()) {
        for (const Buffer &b : uniformBuffers.value()) {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.setBuffer(b.buffer);
            bufferInfo.setOffset(0);
            bufferInfo.setRange(b.allocationInfo.size);
            bufferInfos.emplace_back(bufferInfo);
        }
        // A single descriptor write.
        // In principle, we can have multiple and still update everything in a batch
        vk::WriteDescriptorSet unifomWrite{};
        unifomWrite.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        unifomWrite.setDstSet(uniformSet.value());
        unifomWrite.setDstBinding(BINDING_DICT.at(vk::DescriptorType::eUniformBuffer));
        unifomWrite.setDstArrayElement(0);
        unifomWrite.setBufferInfo(bufferInfos);
        descriptorWrites.push_back(unifomWrite);
    }

    // Update TLAS
    vk::WriteDescriptorSetAccelerationStructureKHR tlasWriteKHR{};
    vk::WriteDescriptorSet tlasWrite{};
    if (tlas.has_value() && tlasSet.has_value()) {
        tlasWriteKHR.setAccelerationStructures(tlas.value());
        tlasWrite.setPNext(&tlasWriteKHR);
        tlasWrite.setDescriptorCount(1);
        tlasWrite.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
        tlasWrite.setDstSet(tlasSet.value());
        tlasWrite.setDstBinding(BINDING_DICT.at(vk::DescriptorType::eAccelerationStructureKHR));
        descriptorWrites.push_back(tlasWrite);
    }

    // Update output image
    vk::DescriptorImageInfo imageInfo{};
    vk::WriteDescriptorSet imageWrite{};
    if (imageView.has_value() && imageSet.has_value()) {
        imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
        imageInfo.setImageView(imageView.value());
        imageWrite.setImageInfo(imageInfo);
        imageWrite.setDstSet(imageSet.value());
        imageWrite.setDescriptorType(vk::DescriptorType::eStorageImage);
        imageWrite.setDstBinding(BINDING_DICT.at(vk::DescriptorType::eStorageImage));
        descriptorWrites.push_back(imageWrite);
    }

    device.updateDescriptorSets(descriptorWrites, nullptr);
}
