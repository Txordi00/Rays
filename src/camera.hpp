#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "types.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

class Camera
{
public:
    Camera() = default;
    ~Camera() = default;

    void setProjMatrix(
        const float &fov, const float &w, const float &h, const float &near, const float &far);

    void forward(const float &dx);
    void backwards(const float &dx);
    void right(const float &dx);
    void left(const float &dx);
    void up(const float &dx);
    void down(const float &dx);

    void lookUp(const float &dx);
    void lookDown(const float &dx);
    void lookRight(const float &dx);
    void lookLeft(const float &dx);

    void update();

    void create_camera_storage_buffer(const vk::Device &device, const VmaAllocator &allocator);
    void destroy_camera_storage_buffer();

    glm::mat4 viewMatrix{1.f};
    glm::vec3 orientation{0, 0, 1};
    glm::vec3 translation{0.f};
    glm::mat4 projMatrix{1.f};

    Buffer cameraBuffer;
    CameraData cameraData{};

private:
    VmaAllocator allocator;
};
