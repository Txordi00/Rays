#pragma once

#include "types.hpp"
#include <glm/glm.hpp>

enum LightType { ePoint, eDirectional };

class Light
{
public:
    struct LightData
    {
        glm::vec3 position{0.f};
        glm::vec3 color{1.f};
        float intensity{1.f};
        uint32_t type{LightType::ePoint};
    };

    Light(const vk::Device &device, const VmaAllocator &allocator)
        : device{device}
        , allocator{allocator}
    {}
    ~Light();

    void upload();
    void update();
    void destroy();

    LightData lightData{};

    Buffer ubo;

private:
    const vk::Device &device;
    const VmaAllocator &allocator;
};
