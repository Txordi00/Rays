#include "uniform_buffers.hpp"

void Ubo::destroy()
{
    device.destroyDescriptorPool(pool);
}

void Ubo::create_descriptor_pool(const uint32_t maxDescriptorCount, const uint32_t maxSets)
{
    maxDescriptors = maxDescriptorCount;
    vk::DescriptorPoolSize poolSize{};
    poolSize.setType(vk::DescriptorType::eUniformBuffer);
    poolSize.setDescriptorCount(maxDescriptorCount);
    std::vector<vk::DescriptorPoolSize> poolSizes(maxSets, poolSize);

    vk::DescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);
    poolCreateInfo.setMaxSets(maxSets);
    poolCreateInfo.setPoolSizes(poolSizes);

    pool = device.createDescriptorPool(poolCreateInfo);
}

vk::DescriptorSetLayout Ubo::create_descriptor_set_layout(const vk::ShaderStageFlags &shaderStages)
{
    vk::DescriptorSetLayoutBinding binding{};
    binding.setBinding(0);
    binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    binding.setDescriptorCount(maxDescriptors);
    binding.setStageFlags(shaderStages);

    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    vk::DescriptorBindingFlags bindingFlags{vk::DescriptorBindingFlagBits::ePartiallyBound
                                            | vk::DescriptorBindingFlagBits::eUpdateAfterBind};
    bindingFlagsInfo.setBindingFlags(bindingFlags);

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.setBindings(binding);
    descriptorSetLayoutInfo.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
    descriptorSetLayoutInfo.setPNext(bindingFlagsInfo);

    return device.createDescriptorSetLayout(descriptorSetLayoutInfo);
}

// CHECK THAT descriptorSetCount == return.size()
std::vector<vk::DescriptorSet> Ubo::allocate_descriptor_sets(
    const vk::DescriptorSetLayout &descriptorSetLayout, const uint32_t descriptorSetCount)
{
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(pool);
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts(descriptorSetCount,
                                                              descriptorSetLayout);
    allocInfo.setSetLayouts(descriptorSetLayouts);

    return device.allocateDescriptorSets(allocInfo);
}

void Ubo::update_descriptor_sets(const std::vector<Buffer> &buffers,
                                 const vk::DescriptorSet &descriptorSet)
{
    // Add all the buffers to a single descriptor write
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(buffers.size());
    for (const Buffer &b : buffers) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.setBuffer(b.buffer);
        bufferInfo.setOffset(0);
        bufferInfo.setRange(b.allocationInfo.size);
        bufferInfos.emplace_back(bufferInfo);
    }
    // A single descriptor write.
    // In principle, we can have multiple and still update everything in a batch
    vk::WriteDescriptorSet descriptorWrite{};
    descriptorWrite.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    descriptorWrite.setDstSet(descriptorSet);
    descriptorWrite.setDstBinding(0);
    descriptorWrite.setDstArrayElement(0);
    descriptorWrite.setBufferInfo(bufferInfos); // Weird that I can input multiple buffer infos here

    device.updateDescriptorSets(descriptorWrite, nullptr);
}
