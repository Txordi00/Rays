#pragma once

#include "types.hpp"
#include <glm/glm.hpp>

enum LightType { ePoint, eDirectional };

class Light
{
public:
    struct LightData
    {
        glm::vec3 positionOrDirection{0.f};
        glm::vec3 color{1.f};
        float intensity{1.f};
        uint32_t type{LightType::ePoint};
    };

    Light()
        : id_{nextId++}
    {}
    ~Light() = default;

    void upload(const vk::Device &device, const VmaAllocator &allocator);
    void update();
    void destroy();

    uint32_t id() const { return id_; }

    LightData lightData{};

    Buffer ubo{};

private:
    uint32_t id_;
    static uint32_t nextId;
    VmaAllocator allocator;
};

class LightsManager
{
public:
    LightsManager(const vk::Device &device, const VmaAllocator &allocator)
        : device{device}
        , allocator{allocator}
    {}

    void run();

    std::vector<Light> lights;
    std::vector<Buffer> lightBuffers;

private:
    const vk::Device &device;
    const VmaAllocator &allocator;

    uint32_t nextId{0};
};
