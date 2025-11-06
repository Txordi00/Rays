#include "lights.hpp"
#include "utils.hpp"
#include <print>

Light::~Light()
{
    if (ubo.buffer)
        destroy();
}

void Light::upload()
{
    if (!ubo.buffer) {
        ubo = utils::create_buffer(device,
                                   allocator,
                                   sizeof(Light::LightData),
                                   vk::BufferUsageFlagBits::eUniformBuffer,
                                   VMA_MEMORY_USAGE_AUTO,
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                       | VMA_ALLOCATION_CREATE_MAPPED_BIT);
        utils::copy_to_buffer(ubo, allocator, &lightData);
    } else
        std::println("Light buffer already created.");
}

void Light::update()
{
    utils::copy_to_buffer(ubo, allocator, &lightData);
}

void Light::destroy()
{
    utils::destroy_buffer(allocator, ubo);
}
