#include "descriptors.hpp"

DescriptorSetLayout::DescriptorSetLayout(const vk::Device &device)
    : device{device}
{}

void DescriptorSetLayout::add_binding(vk::DescriptorSetLayoutBinding binding)
{
    bindings.push_back(binding);
}

vk::DescriptorSetLayout DescriptorSetLayout::get_descriptor_set_layout(
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
                               const std::vector<DescriptorSetData> &descriptorSets,
                               const uint32_t maxSets)
    : device{device}
    , descriptorSets{descriptorSets}
    , maxSets{maxSets}
{
    // This is how I understand it:
    assert(descriptorSets.size() <= maxSets);
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

// DescriptorPool::~DescriptorPool()
// {
//     destroyPool();
// }

vk::DescriptorPool DescriptorPool::create(
    const vk::DescriptorPoolCreateFlags &descriptorPoolCreateFlags)
{
    std::vector<vk::DescriptorPoolSize> poolSizes(descriptorSets.size());
    // poolSizes.resize(descriptorSets.size());
    for (int i = 0; i < descriptorSets.size(); i++) {
        poolSizes[i].setType(descriptorSets[i].type);
        poolSizes[i].setDescriptorCount(descriptorSets[i].descriptorCount);
    }

    vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{};
    descriptorPoolCreateInfo.setPoolSizes(poolSizes);
    descriptorPoolCreateInfo.setMaxSets(maxSets);
    descriptorPoolCreateInfo.setFlags(descriptorPoolCreateFlags);

    vk::DescriptorSetAllocateInfo a{};

    pool = device.createDescriptorPool(descriptorPoolCreateInfo);
    return pool;
}

std::vector<vk::DescriptorSet> DescriptorPool::allocate_descriptors(
    const std::vector<unsigned int> &indexes)
{
    // Select the vk::DescriptorSetLayout's to allocate
    std::vector<vk::DescriptorSetLayout> descriptorSetLayouts(indexes.size());
    for (int i = 0; i < indexes.size(); i++)
        descriptorSetLayouts[i] = descriptorSets[indexes[i]].layout;

    vk::DescriptorSetAllocateInfo descriptorSetAllocInfo{};
    descriptorSetAllocInfo.setDescriptorPool(pool);
    descriptorSetAllocInfo.setSetLayouts(descriptorSetLayouts);

    return device.allocateDescriptorSets(descriptorSetAllocInfo);
}

void DescriptorPool::reset()
{
    device.resetDescriptorPool(pool);
}

void DescriptorPool::destroyPool()
{
    device.destroyDescriptorPool(pool);
}
