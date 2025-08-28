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

void DescHelper::add_binding(const Binding &bind)
{
    uint32_t maxDescriptors = 0;
    for (const auto &ps : poolSizes)
        if (bind.type == ps.type)
            maxDescriptors = std::max(maxDescriptors, ps.descriptorCount);

    vk::DescriptorSetLayoutBinding vkBinding{};
    vkBinding.setBinding(bind.binding);
    vkBinding.setDescriptorType(bind.type);
    vkBinding.setDescriptorCount(maxDescriptors);
    vkBinding.setStageFlags(bind.shaderStageFlags);

    bindings.push_back(vkBinding);
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

// I SHOULD USE TEMPLATES HERE: vector<vk::DescriptorSet> sets, vector<uint32_t> bindings,
// T_1...T_sets.size() setData
void update_descriptor_sets(const vk::Device &device,
                            const std::optional<std::vector<Buffer> > &uniformBuffers,
                            const std::optional<vk::DescriptorSet> &uniformSet,
                            const std::optional<uint32_t> uBinding,
                            const std::optional<vk::AccelerationStructureKHR> &tlas,
                            const std::optional<vk::DescriptorSet> &tlasSet,
                            const std::optional<uint32_t> asBinding,
                            const std::optional<vk::ImageView> &imageView,
                            const std::optional<vk::DescriptorSet> &imageSet,
                            const std::optional<uint32_t> imBinding)
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
            bufferInfo.setRange(vk::WholeSize);
            bufferInfos.emplace_back(bufferInfo);
        }
        // A single descriptor write.
        // In principle, we can have multiple and still update everything in a batch
        vk::WriteDescriptorSet unifomWrite{};
        unifomWrite.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        unifomWrite.setDstSet(uniformSet.value());
        unifomWrite.setDstBinding(uBinding.value());
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
        tlasWrite.setDstBinding(asBinding.value());
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
        imageWrite.setDstBinding(imBinding.value());
        descriptorWrites.push_back(imageWrite);
    }

    device.updateDescriptorSets(descriptorWrites, nullptr);
}

void DescriptorUpdater::add_uniform(const vk::DescriptorSet &descSet,
                                    const uint32_t binding,
                                    const std::vector<Buffer> &uniformBuffers)
{ // Add all the buffers to a single descriptor write
    bufferInfos.reserve(uniformBuffers.size());
    for (const Buffer &b : uniformBuffers) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.setBuffer(b.buffer);
        bufferInfo.setOffset(0);
        bufferInfo.setRange(vk::WholeSize);
        bufferInfos.push_back(bufferInfo);
    }
    // A single descriptor write.
    // In principle, we can have multiple and still update everything in a batch
    vk::WriteDescriptorSet uniformWrite{};
    uniformWrite.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    // uniformWrite.setDescriptorCount(uniformBuffers.size());
    uniformWrite.setDstSet(descSet);
    uniformWrite.setDstBinding(binding);
    uniformWrite.setDstArrayElement(0);
    uniformWrite.setBufferInfo(bufferInfos);
    descriptorWrites.push_back(uniformWrite);
}

void DescriptorUpdater::add_as(const vk::DescriptorSet &descSet,
                               const uint32_t binding,
                               const vk::AccelerationStructureKHR &as)
{ // Update TLAS
    vk::WriteDescriptorSet tlasWrite{};
    tlasWriteKHR.setAccelerationStructures(as);
    tlasWrite.setPNext(&tlasWriteKHR);
    tlasWrite.setDescriptorCount(1);
    tlasWrite.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
    tlasWrite.setDstSet(descSet);
    tlasWrite.setDstBinding(binding);
    descriptorWrites.push_back(tlasWrite);
}

void DescriptorUpdater::add_image(const vk::DescriptorSet &descSet,
                                  const uint32_t binding,
                                  const vk::ImageView &imageView)
{
    // Update output image
    vk::WriteDescriptorSet imageWrite{};
    imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
    imageInfo.setImageView(imageView);
    imageWrite.setImageInfo(imageInfo);
    imageWrite.setDstSet(descSet);
    imageWrite.setDescriptorType(vk::DescriptorType::eStorageImage);
    imageWrite.setDstBinding(binding);
    descriptorWrites.push_back(imageWrite);
}

void DescriptorUpdater::update()
{
    device.updateDescriptorSets(descriptorWrites, nullptr);
    // bufferInfos = {};
    // imageInfo = vk::DescriptorImageInfo{};
    // tlasWriteKHR = vk::WriteDescriptorSetAccelerationStructureKHR{};
}
