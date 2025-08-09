#include "uniform_buffers.hpp"

Ubo::Ubo(const vk::Device &device)
    : device{device}
{}

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

// vk::PipelineLayout Ubo::create_pipeline_layout(const vk::PushConstantRange &pushRange,
//                                                const vk::DescriptorSetLayout &descriptorSetLayout)
// {
//     vk::PipelineLayoutCreateInfo createInfo{};
//     createInfo.setSetLayouts(descriptorSetLayout);
//     createInfo.setPushConstantRanges(pushRange);

//     return device.createPipelineLayout(createInfo);
// }

void Ubo::update_buffer(const Buffer &buffer, const void *data)
{
    assert(buffer.allocationInfo.pMappedData && "Cannot copy to unmapped buffer");
    memcpy(buffer.allocationInfo.pMappedData, data, buffer.allocationInfo.size);
}

void Ubo::update_descriptor_sets(const std::vector<Buffer> &buffers,
                                 const std::vector<vk::DescriptorSet> &descriptorSets)
{
    assert(buffers.size() == descriptorSets.size()
           && "Number of buffers != number of descriptor sets");
    // std::vector<vk::DescriptorBufferInfo> bufferInfos(buffers.size());
    std::vector<vk::WriteDescriptorSet> descriptorWrites(buffers.size());
    for (int i = 0; i < buffers.size(); i++) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.setBuffer(buffers[i].buffer);
        bufferInfo.setOffset(0);
        bufferInfo.setRange(vk::WholeSize);
        // bufferInfos.emplace_back(bufferInfo);

        vk::WriteDescriptorSet descriptorWrite{};
        descriptorWrite.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        descriptorWrite.setDstSet(descriptorSets[i]);
        descriptorWrite.setDstBinding(0);
        descriptorWrite.setDstArrayElement(buffers[i].bufferId);
        descriptorWrite.setDescriptorCount(1);
        descriptorWrite.setBufferInfo(
            bufferInfo); // Weird that I can input multiple buffer infos here
        descriptorWrites[i] = descriptorWrite;
    }

    device.updateDescriptorSets(descriptorWrites, nullptr);
}
