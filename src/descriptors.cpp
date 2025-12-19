#include "descriptors.hpp"
#include <tuple>

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
    // uint32_t numUniformDescriptors = 0, numStorageImageDescriptors = 0, numASDescriptors = 0;
    vk::DescriptorPoolSize actualPoolSize{poolSize};
    actualPoolSize.setDescriptorCount(std::max<uint32_t>(poolSize.descriptorCount, 1));
    poolSizes.reserve(numSets);
    for (int i = 0; i < numSets; i++) {
        poolSizes.emplace_back(actualPoolSize);
    }
}

void DescHelper::create_descriptor_pool()
{
    vk::DescriptorPoolCreateInfo poolCreateInfo{};
    // Set +1 to the shader printf function
    poolCreateInfo.setMaxSets(poolSizes.size() + 1);
    poolCreateInfo.setPoolSizes(poolSizes);
    if (updateAfterBind)
        poolCreateInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

    pool = device.createDescriptorPool(poolCreateInfo);
}

void DescHelper::add_binding(const Binding &bind)
{
    vk::DescriptorSetLayoutBinding vkBinding{};
    vkBinding.setBinding(bind.binding);
    vkBinding.setDescriptorType(bind.type);
    vkBinding.setDescriptorCount(std::max<uint32_t>(bind.descriptorCount, 1));
    vkBinding.setStageFlags(bind.shaderStageFlags);

    bindings.push_back(vkBinding);
}

// Returns a pair (bindless ds layout, rt ds layout)
vk::DescriptorSetLayout DescHelper::create_descriptor_set_layout()
{
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.setBindings(bindings);

    std::vector<vk::DescriptorBindingFlags>
        bindlessBindingFlags{bindings.size(),
                             vk::DescriptorBindingFlags{
                                 vk::DescriptorBindingFlagBits::ePartiallyBound
                                 | vk::DescriptorBindingFlagBits::eUpdateAfterBind}};
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

void DescriptorUpdater::add_uniform(const vk::DescriptorSet &descSet,
                                    const uint32_t binding,
                                    const std::vector<Buffer> &uniformBuffers)
{
    // Add all the buffers to a single descriptor write
    std::vector<vk::DescriptorBufferInfo> bufferInfosTmp;
    bufferInfosTmp.reserve(uniformBuffers.size());
    for (const Buffer &b : uniformBuffers) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.setBuffer(b.buffer);
        bufferInfo.setOffset(0);
        bufferInfo.setRange(vk::WholeSize);
        bufferInfosTmp.emplace_back(bufferInfo);
    }
    const auto bufferInfosTupleTmp = std::make_tuple(descSet, binding, bufferInfosTmp);
    uniformInfos.push_back(bufferInfosTupleTmp);
    descriptorCount += uniformBuffers.size();
}

void DescriptorUpdater::add_storage(const vk::DescriptorSet &descSet,
                                    const uint32_t binding,
                                    const std::vector<Buffer> &storageBuffers)
{
    // Add all the buffers to a single descriptor write
    std::vector<vk::DescriptorBufferInfo> bufferInfosTmp;
    bufferInfosTmp.reserve(storageBuffers.size());
    for (const Buffer &b : storageBuffers) {
        vk::DescriptorBufferInfo bufferInfo{};
        bufferInfo.setBuffer(b.buffer);
        bufferInfo.setOffset(0);
        bufferInfo.setRange(vk::WholeSize);
        bufferInfosTmp.emplace_back(bufferInfo);
    }
    const auto bufferInfosTupleTmp = std::make_tuple(descSet, binding, bufferInfosTmp);
    storageInfos.push_back(bufferInfosTupleTmp);
    descriptorCount += storageBuffers.size();
}

void DescriptorUpdater::add_as(const vk::DescriptorSet &descSet,
                               const uint32_t binding,
                               const vk::AccelerationStructureKHR &as)
{
    // Update TLAS
    vk::WriteDescriptorSetAccelerationStructureKHR tlasWriteKHR{as};
    const auto tlasWriteKHRTupleTmp = std::make_tuple(descSet, binding, tlasWriteKHR);
    tlasWritesKHR.push_back(tlasWriteKHRTupleTmp);
    descriptorCount += 1;
}

void DescriptorUpdater::add_storage_image(const vk::DescriptorSet &descSet,
                                          const uint32_t binding,
                                          const std::vector<ImageData> &images)
{
    std::vector<vk::DescriptorImageInfo> imageInfosTmp;
    imageInfosTmp.reserve(images.size());
    for (const auto &im : images) {
        vk::DescriptorImageInfo imageInfo{};
        imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
        imageInfo.setImageView(im.imageView);
        imageInfo.setSampler(nullptr);
        imageInfosTmp.emplace_back(imageInfo);
    }
    const auto imageInfoTumpleTmp = std::make_tuple(descSet, binding, imageInfosTmp);
    imageStorageInfos.push_back(imageInfoTumpleTmp);
    descriptorCount += images.size();
}

void DescriptorUpdater::add_sampled_image(const vk::DescriptorSet &descSet,
                                          const uint32_t binding,
                                          const std::vector<ImageData> &images)
{
    std::vector<vk::DescriptorImageInfo> imageInfosTmp;
    imageInfosTmp.reserve(images.size());
    for (const auto &im : images) {
        vk::DescriptorImageInfo imageInfo{};
        imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
        imageInfo.setImageView(im.imageView);
        imageInfo.setSampler(nullptr);
        imageInfosTmp.emplace_back(imageInfo);
    }
    const auto imageInfoTumpleTmp = std::make_tuple(descSet, binding, imageInfosTmp);
    imageSampledInfos.push_back(imageInfoTumpleTmp);
    descriptorCount += images.size();
}

void DescriptorUpdater::add_combined_image(const vk::DescriptorSet &descSet,
                                           const uint32_t binding,
                                           const std::vector<ImageData> &images)
{
    std::vector<vk::DescriptorImageInfo> imageInfosTmp;
    imageInfosTmp.reserve(images.size());
    for (const auto &im : images) {
        assert(im.sampler && "Sampler missing");
        vk::DescriptorImageInfo imageInfo{};
        imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
        imageInfo.setImageView(im.imageView);
        imageInfo.setSampler(im.sampler);
        imageInfosTmp.emplace_back(imageInfo);
    }
    const auto imageInfoTumpleTmp = std::make_tuple(descSet, binding, imageInfosTmp);
    combinedImageInfos.push_back(imageInfoTumpleTmp);
    descriptorCount += images.size();
}

void DescriptorUpdater::add_sampler(const vk::DescriptorSet &descSet,
                                    const uint32_t binding,
                                    const std::vector<vk::Sampler> &samplers)
{
    std::vector<vk::DescriptorImageInfo> samplerInfosTmp;
    samplerInfosTmp.reserve(samplers.size());
    for (const auto &s : samplers) {
        vk::DescriptorImageInfo samplerInfo{};
        samplerInfo.setImageLayout(vk::ImageLayout::eUndefined);
        samplerInfo.setImageView(nullptr);
        samplerInfo.setSampler(s);
        samplerInfosTmp.emplace_back(samplerInfo);
    }
    const auto samplerInfoTumpleTmp = std::make_tuple(descSet, binding, samplerInfosTmp);
    samplerInfos.push_back(samplerInfoTumpleTmp);
    descriptorCount += samplers.size();
}

void DescriptorUpdater::update()
{
    std::vector<vk::WriteDescriptorSet> descriptorWrites;
    descriptorWrites.reserve(descriptorCount);
    for (const auto &bi : uniformInfos) {
        vk::WriteDescriptorSet uniformWrite{};
        uniformWrite.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        uniformWrite.setDstSet(std::get<0>(bi));
        uniformWrite.setDstBinding(std::get<1>(bi));
        uniformWrite.setBufferInfo(std::get<2>(bi));
        uniformWrite.setDstArrayElement(0);
        descriptorWrites.emplace_back(uniformWrite);
    }
    for (const auto &bi : storageInfos) {
        vk::WriteDescriptorSet storageWrite{};
        storageWrite.setDescriptorType(vk::DescriptorType::eStorageBuffer);
        storageWrite.setDstSet(std::get<0>(bi));
        storageWrite.setDstBinding(std::get<1>(bi));
        storageWrite.setBufferInfo(std::get<2>(bi));
        storageWrite.setDstArrayElement(0);
        descriptorWrites.emplace_back(storageWrite);
    }
    for (const auto &ii : imageStorageInfos) {
        vk::WriteDescriptorSet imageWrite{};
        imageWrite.setDstSet(std::get<0>(ii));
        imageWrite.setDstBinding(std::get<1>(ii));
        imageWrite.setImageInfo(std::get<2>(ii));
        imageWrite.setDescriptorType(vk::DescriptorType::eStorageImage);
        descriptorWrites.emplace_back(imageWrite);
    }
    for (const auto &ii : imageSampledInfos) {
        vk::WriteDescriptorSet imageWrite{};
        imageWrite.setDstSet(std::get<0>(ii));
        imageWrite.setDstBinding(std::get<1>(ii));
        imageWrite.setImageInfo(std::get<2>(ii));
        imageWrite.setDescriptorType(vk::DescriptorType::eSampledImage);
        descriptorWrites.emplace_back(imageWrite);
    }
    for (const auto &ii : combinedImageInfos) {
        vk::WriteDescriptorSet imageWrite{};
        imageWrite.setDstSet(std::get<0>(ii));
        imageWrite.setDstBinding(std::get<1>(ii));
        imageWrite.setImageInfo(std::get<2>(ii));
        imageWrite.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        descriptorWrites.emplace_back(imageWrite);
    }
    for (const auto &ii : samplerInfos) {
        vk::WriteDescriptorSet samplerWrite{};
        samplerWrite.setDstSet(std::get<0>(ii));
        samplerWrite.setDstBinding(std::get<1>(ii));
        samplerWrite.setImageInfo(std::get<2>(ii));
        samplerWrite.setDescriptorType(vk::DescriptorType::eSampler);
        descriptorWrites.emplace_back(samplerWrite);
    }
    for (const auto &tw : tlasWritesKHR) {
        vk::WriteDescriptorSet tlasWrite{};
        tlasWrite.setDstSet(std::get<0>(tw));
        tlasWrite.setDstBinding(std::get<1>(tw));
        tlasWrite.setPNext(&std::get<2>(tw));
        tlasWrite.setDescriptorCount(1);
        tlasWrite.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
        descriptorWrites.emplace_back(tlasWrite);
    }
    device.updateDescriptorSets(descriptorWrites, nullptr);
    clean();
}

void DescriptorUpdater::clean()
{
    uniformInfos = {};
    storageInfos = {};
    imageStorageInfos = {};
    imageSampledInfos = {};
    samplerInfos = {};
    tlasWritesKHR = {};
    descriptorCount = 0;
}
