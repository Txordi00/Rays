#include "rt_descriptors.hpp"

vk::DescriptorSetLayout RtDescriptors::create_rt_descriptor_set_layout()
{
    // Ray tracing specific descriptors (set 0)
    std::vector<vk::DescriptorSetLayoutBinding> rtBindings;

    // TLAS binding - accessible from raygen and closest hit
    vk::DescriptorSetLayoutBinding tlasBinding{};
    tlasBinding.setBinding(BINDING_TLAS);
    tlasBinding.setDescriptorCount(1);
    tlasBinding.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR);
    tlasBinding.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR
                              | vk::ShaderStageFlagBits::eClosestHitKHR);
    rtBindings.push_back(tlasBinding);

    // Output image binding - only accessible from raygen
    vk::DescriptorSetLayoutBinding outImageBinding{};
    outImageBinding.setBinding(BINDING_OUT_IMG);
    outImageBinding.setDescriptorCount(1);
    outImageBinding.setDescriptorType(vk::DescriptorType::eStorageImage);
    outImageBinding.setStageFlags(vk::ShaderStageFlagBits::eRaygenKHR);
    rtBindings.push_back(outImageBinding);

    // Create the layout
    // vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    // vk::DescriptorBindingFlags bindingFlags{vk::DescriptorBindingFlagBits::ePartiallyBound
    //                                         | vk::DescriptorBindingFlagBits::eUpdateAfterBind};
    // bindingFlagsInfo.setBindingFlags(bindingFlags);

    vk::DescriptorSetLayoutCreateInfo rtLayoutInfo{};
    rtLayoutInfo.setBindings(rtBindings);
    // rtLayoutInfo.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
    // rtLayoutInfo.setPNext(&bindingFlagsInfo);

    return device.createDescriptorSetLayout(rtLayoutInfo);
}
