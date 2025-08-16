#include "descriptors2.hpp"

Ubo::Ubo(const vk::Device &device,
         const vk::PhysicalDeviceProperties &physDevProp,
         const vk::PhysicalDeviceAccelerationStructurePropertiesKHR &asProperties)
    : device{device}
    , physDevProp{physDevProp}
    , asProperties{asProperties}
{}

void Ubo::destroy()
{
    device.destroyDescriptorPool(poolBindless);
    device.destroyDescriptorPool(poolRt);
}

void Ubo::create_descriptor_pools(const std::vector<vk::DescriptorPoolSize> &poolSizes)
{
    uint32_t numUniformDescriptors = 0, numStorageImageDescriptors = 0, numASDescriptors = 0;
    this->poolSizes = poolSizes;
    std::vector<vk::DescriptorPoolSize> poolSizesBindless;
    std::vector<vk::DescriptorPoolSize> poolSizesRt;

    for (const auto &ps : poolSizes) {
        if (ps.type == vk::DescriptorType::eUniformBuffer) {
            numUniformDescriptors++;
            maxUniformDescriptors = (maxUniformDescriptors < ps.descriptorCount)
                                        ? ps.descriptorCount
                                        : maxUniformDescriptors;
            poolSizesBindless.push_back(ps);
        } else if (ps.type == vk::DescriptorType::eStorageImage) {
            numStorageImageDescriptors++;
            maxStorageImageDescriptors = (maxStorageImageDescriptors < ps.descriptorCount)
                                             ? ps.descriptorCount
                                             : maxStorageImageDescriptors;
            poolSizesRt.push_back(ps);
        } else if (ps.type == vk::DescriptorType::eAccelerationStructureKHR) {
            numASDescriptors++;
            maxASDescriptors = (maxASDescriptors < ps.descriptorCount) ? ps.descriptorCount
                                                                       : maxASDescriptors;
            poolSizesRt.push_back(ps);
        }
    }
    assert(numUniformDescriptors <= physDevProp.limits.maxDescriptorSetUniformBuffers);
    assert(numStorageImageDescriptors <= physDevProp.limits.maxDescriptorSetStorageImages);
    assert(numASDescriptors <= asProperties.maxDescriptorSetAccelerationStructures);

    vk::DescriptorPoolCreateInfo poolBindlessCreateInfo{};
    poolBindlessCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);
    poolBindlessCreateInfo.setMaxSets(poolSizesBindless.size());
    poolBindlessCreateInfo.setPoolSizes(poolSizesBindless);

    vk::DescriptorPoolCreateInfo poolRtCreateInfo{};
    poolRtCreateInfo.setMaxSets(poolSizesRt.size());
    poolRtCreateInfo.setPoolSizes(poolSizesRt);

    poolBindless = device.createDescriptorPool(poolBindlessCreateInfo);
    poolRt = device.createDescriptorPool(poolRtCreateInfo);
}

// Returns a pair (bindless ds layout, rt ds layout)
std::pair<vk::DescriptorSetLayout, vk::DescriptorSetLayout> Ubo::create_descriptor_set_layouts()
{
    std::vector<vk::DescriptorSetLayoutBinding> bindlessBindings;

    vk::DescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.setBinding(BINDING_UNIFORM);
    uniformBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    uniformBinding.setDescriptorCount(maxUniformDescriptors);
    uniformBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
    bindlessBindings.push_back(uniformBinding);

    std::vector<vk::DescriptorSetLayoutBinding> rtBindings;
    // TLAS binding - accessible from raygen and closest hit
    vk::DescriptorSetLayoutBinding tlasBinding{};
    tlasBinding.setBinding(BINDING_TLAS);
    tlasBinding.setDescriptorCount(maxASDescriptors);
    tlasBinding.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
    tlasBinding.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR
                              | vk::ShaderStageFlagBits::eClosestHitKHR);
    rtBindings.push_back(tlasBinding);

    // Output image binding - only accessible from raygen
    vk::DescriptorSetLayoutBinding outImageBinding{};
    outImageBinding.setBinding(BINDING_OUT_IMG);
    outImageBinding.setDescriptorCount(maxStorageImageDescriptors);
    outImageBinding.setDescriptorType(vk::DescriptorType::eStorageImage);
    outImageBinding.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);
    rtBindings.push_back(outImageBinding);

    vk::DescriptorBindingFlags bindlessBindingFlags{
        vk::DescriptorBindingFlagBits::ePartiallyBound
        | vk::DescriptorBindingFlagBits::eUpdateAfterBind};
    vk::DescriptorSetLayoutBindingFlagsCreateInfo bindlessBindingFlagsInfo{};
    bindlessBindingFlagsInfo.setBindingFlags(bindlessBindingFlags);

    vk::DescriptorSetLayoutCreateInfo bindlesssDescriptorSetLayoutInfo{};
    bindlesssDescriptorSetLayoutInfo.setBindings(bindlessBindings);
    bindlesssDescriptorSetLayoutInfo.setFlags(
        vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
    bindlesssDescriptorSetLayoutInfo.setPNext(bindlessBindingFlagsInfo);

    vk::DescriptorSetLayoutCreateInfo rtDescriptorSetLayoutInfo{};
    rtDescriptorSetLayoutInfo.setBindings(rtBindings);

    vk::DescriptorSetLayout bindlessDescriptorSetLayout = device.createDescriptorSetLayout(
        bindlesssDescriptorSetLayoutInfo);

    vk::DescriptorSetLayout rtDescriptorSetLayout = device.createDescriptorSetLayout(
        rtDescriptorSetLayoutInfo);

    return std::pair<vk::DescriptorSetLayout, vk::DescriptorSetLayout>(bindlessDescriptorSetLayout,
                                                                       rtDescriptorSetLayout);
}

std::vector<vk::DescriptorSet> Ubo::allocate_descriptor_sets(
    const std::pair<vk::DescriptorSetLayout, vk::DescriptorSetLayout> &descriptorSetLayouts,
    const uint32_t frameOverlap)
{
    std::vector<vk::DescriptorSetLayout> bindlessDescriptorSetLayouts(frameOverlap,
                                                                      descriptorSetLayouts.first);
    std::vector<vk::DescriptorSetLayout> rtDescriptorSetLayouts(frameOverlap,
                                                                descriptorSetLayouts.second);

    // std::vector<vk::DescriptorSetLayout> allDescriptorSetLayouts;
    // allDescriptorSetLayouts.resize(bindlessDescriptorSetLayouts.size()
    //                                + rtDescriptorSetLayouts.size());
    // allDescriptorSetLayouts.insert(allDescriptorSetLayouts.end(),
    //                                bindlessDescriptorSetLayouts.begin(),
    //                                bindlessDescriptorSetLayouts.end());
    // allDescriptorSetLayouts.insert(allDescriptorSetLayouts.end(),
    //                                rtDescriptorSetLayouts.begin(),
    //                                rtDescriptorSetLayouts.end());

    vk::DescriptorSetAllocateInfo allocInfoBindless{};
    allocInfoBindless.setDescriptorPool(poolBindless);
    allocInfoBindless.setSetLayouts(bindlessDescriptorSetLayouts);

    vk::DescriptorSetAllocateInfo allocInfoRt{};
    allocInfoRt.setDescriptorPool(poolRt);
    allocInfoRt.setSetLayouts(rtDescriptorSetLayouts);
    device.allocateDescriptorSets(allocInfoRt);

    return device.allocateDescriptorSets(allocInfoBindless);
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
    descriptorWrite.setBufferInfo(bufferInfos);

    device.updateDescriptorSets(descriptorWrite, nullptr);
}
