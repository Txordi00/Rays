#include "descriptors.hpp"

DescriptorSetLayout::DescriptorSetLayout(const vk::Device &device)
    : device{device}
{}

void DescriptorSetLayout::add_binding(vk::DescriptorSetLayoutBinding binding)
{
    bindings.push_back(binding);
}

vk::DescriptorSetLayout DescriptorSetLayout::get_descriptor_set(
    vk::DescriptorSetLayoutCreateFlags descriptorSetLayoutCreateFlags)
{
    assert(bindings.size() > 0);
    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.setBindingCount(bindings.size());
    descriptorSetLayoutCreateInfo.setBindings(bindings);
    descriptorSetLayoutCreateInfo.setFlags(descriptorSetLayoutCreateFlags);

    layout = device.createDescriptorSetLayout(descriptorSetLayoutCreateInfo);
    return layout;
}

void DescriptorSetLayout::reset()
{
    bindings.clear();
}

DescriptorPool::DescriptorPool(const vk::Device &device,
                               const std::vector<DescriptorData> &descriptors,
                               const uint32_t maxSets)
    : device{device}
    , descriptors{descriptors}
    , maxSets{maxSets}
{
    // Not correct. Comparing different things:
    // // Check that we have less descriptors than
    // uint32_t totalNumDescriptors = std::accumulate(descriptorSets.begin(),
    //                                                descriptorSets.end(),
    //                                                0,
    //                                                [](uint32_t sum, const DescriptorSet &d) {
    //                                                    return sum + d.numDescriptors;
    //                                                });
    // assert(maxSets <= totalNumDescriptors);
}

DescriptorPool::~DescriptorPool()
{
    device.destroyDescriptorPool(pool);
}

vk::DescriptorPool DescriptorPool::create(
    const vk::DescriptorPoolCreateFlags &descriptorPoolCreateFlags)
{
    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.resize(descriptors.size());
    for (int i = 0; i < descriptors.size(); i++) {
        poolSizes[i].setType(descriptors[i].type);
        poolSizes[i].setDescriptorCount(descriptors[i].descriptorCount);
    }

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.setPoolSizes(poolSizes);
    descriptorPoolCreateInfo.setMaxSets(maxSets);
    descriptorPoolCreateInfo.setFlags(descriptorPoolCreateFlags);

    vk::DescriptorSetAllocateInfo a{};

    pool = device.createDescriptorPool(descriptorPoolCreateInfo);
    return pool;
}

void DescriptorPool::reset()
{
    device.resetDescriptorPool(pool);
}
