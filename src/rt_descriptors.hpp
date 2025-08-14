#pragma once

#include "types.hpp"

const uint32_t BINDING_TLAS = 0;
const uint32_t BINDING_OUT_IMG = 0;

class RtDescriptors
{
public:
    RtDescriptors(const vk::Device &device)
        : device{device}
    {}

    vk::DescriptorSetLayout create_rt_descriptor_set_layout();

private:
    const vk::Device &device;
    vk::DescriptorPool pool;
};
