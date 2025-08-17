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
    device.destroyDescriptorPool(poolUAB);
    device.destroyDescriptorPool(poolNonUAB);
}

void Ubo::add_descriptor_set(const vk::DescriptorPoolSize &poolSize,
                             const uint32_t numSets,
                             const bool updateAfterBind)
{
    uint32_t numUniformDescriptors = 0, numStorageImageDescriptors = 0, numASDescriptors = 0;
    for (int i = 0; i < numSets; i++) {
        if (updateAfterBind)
            poolSizesUAB.push_back(poolSize);
        else
            poolSizesNonUAB.push_back(poolSize);

        if (poolSize.type == vk::DescriptorType::eUniformBuffer) {
            numUniformDescriptors++;
            maxUniformDescriptors = (maxUniformDescriptors < poolSize.descriptorCount)
                                        ? poolSize.descriptorCount
                                        : maxUniformDescriptors;
        } else if (poolSize.type == vk::DescriptorType::eStorageImage) {
            numStorageImageDescriptors++;
            maxStorageImageDescriptors = (maxStorageImageDescriptors < poolSize.descriptorCount)
                                             ? poolSize.descriptorCount
                                             : maxStorageImageDescriptors;
        } else if (poolSize.type == vk::DescriptorType::eAccelerationStructureKHR) {
            numASDescriptors++;
            maxASDescriptors = (maxASDescriptors < poolSize.descriptorCount)
                                   ? poolSize.descriptorCount
                                   : maxASDescriptors;
        }
    }
    assert(numUniformDescriptors <= physDevProp.limits.maxDescriptorSetUniformBuffers);
    assert(numStorageImageDescriptors <= physDevProp.limits.maxDescriptorSetStorageImages);
    assert(numASDescriptors <= asProperties.maxDescriptorSetAccelerationStructures);
}

void Ubo::create_descriptor_pools()
{
    vk::DescriptorPoolCreateInfo poolBindlessCreateInfo{};
    poolBindlessCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);
    poolBindlessCreateInfo.setMaxSets(poolSizesUAB.size());
    poolBindlessCreateInfo.setPoolSizes(poolSizesUAB);

    vk::DescriptorPoolCreateInfo poolRtCreateInfo{};
    poolRtCreateInfo.setMaxSets(poolSizesNonUAB.size());
    poolRtCreateInfo.setPoolSizes(poolSizesNonUAB);

    poolUAB = device.createDescriptorPool(poolBindlessCreateInfo);
    poolNonUAB = device.createDescriptorPool(poolRtCreateInfo);
}

// Returns a pair (bindless ds layout, rt ds layout)
std::pair<vk::DescriptorSetLayout, vk::DescriptorSetLayout> Ubo::create_descriptor_set_layouts()
{
    std::vector<vk::DescriptorSetLayoutBinding> bindlessBindings;

    vk::DescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.setBinding(BINDING_UNIFORM);
    uniformBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    uniformBinding.setDescriptorCount(maxUniformDescriptors);
    uniformBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex
                                 | vk::ShaderStageFlagBits::eRaygenKHR);
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

std::pair<std::vector<vk::DescriptorSet>, std::vector<vk::DescriptorSet>>
Ubo::allocate_descriptor_sets(
    const std::pair<vk::DescriptorSetLayout, vk::DescriptorSetLayout> &descriptorSetLayouts,
    const uint32_t frameOverlap)
{
    std::vector<vk::DescriptorSetLayout> bindlessDescriptorSetLayouts(frameOverlap,
                                                                      descriptorSetLayouts.first);
    std::vector<vk::DescriptorSetLayout> rtDescriptorSetLayouts(frameOverlap,
                                                                descriptorSetLayouts.second);

    vk::DescriptorSetAllocateInfo allocInfoBindless{};
    allocInfoBindless.setDescriptorPool(poolUAB);
    allocInfoBindless.setSetLayouts(bindlessDescriptorSetLayouts);

    vk::DescriptorSetAllocateInfo allocInfoRt{};
    allocInfoRt.setDescriptorPool(poolNonUAB);
    allocInfoRt.setSetLayouts(rtDescriptorSetLayouts);

    return std::pair<std::vector<vk::DescriptorSet>, std::vector<vk::DescriptorSet>>(
        device.allocateDescriptorSets(allocInfoBindless),
        device.allocateDescriptorSets(allocInfoRt));
}

void Ubo::update_descriptor_sets(const std::vector<Buffer> &uniformBuffers,
                                 const vk::DescriptorSet &uniformSet,
                                 const vk::AccelerationStructureKHR &tlas,
                                 const vk::DescriptorSet &tlasSet,
                                 const vk::ImageView &imageView,
                                 const vk::DescriptorSet &imageSet)
{
    std::vector<vk::WriteDescriptorSet> descriptorWrites{};
    // Add all the buffers to a single descriptor write
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    bufferInfos.reserve(uniformBuffers.size());
    for (const Buffer &b : uniformBuffers) {
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
    unifomWrite.setDstSet(uniformSet);
    unifomWrite.setDstBinding(BINDING_UNIFORM);
    unifomWrite.setDstArrayElement(0);
    unifomWrite.setBufferInfo(bufferInfos);
    if (uniformSet)
        descriptorWrites.push_back(unifomWrite);

    // Update TLAS
    vk::WriteDescriptorSetAccelerationStructureKHR tlasWriteKHR{};
    tlasWriteKHR.setAccelerationStructures(tlas);
    vk::WriteDescriptorSet tlasWrite{};
    tlasWrite.setPNext(&tlasWriteKHR);
    tlasWrite.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
    tlasWrite.setDstSet(tlasSet);
    tlasWrite.setDstBinding(BINDING_TLAS);
    if (tlasSet)
        descriptorWrites.push_back(tlasWrite);

    // Update output image
    vk::DescriptorImageInfo imageInfo{};
    imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
    imageInfo.setImageView(imageView);
    vk::WriteDescriptorSet imageWrite{};
    imageWrite.setPImageInfo(&imageInfo);
    imageWrite.setDstSet(imageSet);
    imageWrite.setDescriptorType(vk::DescriptorType::eStorageImage);
    imageWrite.setDstBinding(BINDING_OUT_IMG);
    if (imageSet)
        descriptorWrites.push_back(imageWrite);

    device.updateDescriptorSets(descriptorWrites, nullptr);
}
