#include "camera.hpp"
#include "utils.hpp"
#include <glm/ext.hpp>

void Camera::setProjMatrix(
    const float &fov, const float &w, const float &h, const float &near, const float &far)
{
    projMatrix = glm::perspective(fov, w / h, near, far);
}

void Camera::forward(const float &dx)
{
    translation += dx * orientation;
}

void Camera::backwards(const float &dx)
{
    translation -= dx * orientation;
}

void Camera::right(const float &dx)
{
    glm::vec3 leftDir = glm::cross(orientation, glm::vec3(0, 1, 0));
    translation -= dx * leftDir;
}

void Camera::left(const float &dx)
{
    glm::vec3 leftDir = glm::cross(orientation, glm::vec3(0, 1, 0));
    translation += dx * leftDir;
}

void Camera::up(const float &dx)
{
    translation -= glm::vec3(0, dx, 0);
}

void Camera::down(const float &dx)
{
    translation += glm::vec3(0, dx, 0);
}

void Camera::lookUp(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::cross(glm::vec3(0, 1, 0), orientation));
    orientation = dr * orientation;
}

void Camera::lookDown(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::cross(glm::vec3(0, -1, 0), orientation));
    orientation = dr * orientation;
}

void Camera::lookRight(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::vec3(0, 1, 0));
    orientation = dr * orientation;
}

void Camera::lookLeft(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::vec3(0, -1, 0));
    orientation = dr * orientation;
}

void Camera::update()
{
    viewMatrix = glm::lookAt(translation, translation + orientation, glm::vec3(0, 1, 0));
    if (cameraBuffer.buffer) {
        cameraData.origin = translation;
        cameraData.orientation = orientation;
        utils::map_to_buffer(cameraBuffer, &cameraData);
    }
}

void Camera::create_camera_storage_buffer(const VmaAllocator &allocator)
{
    cameraBuffer = utils::create_buffer(allocator,
                                        sizeof(CameraData),
                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                        VMA_MEMORY_USAGE_AUTO,
                                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                            | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    cameraData.origin = translation;
    cameraData.orientation = orientation;

    utils::map_to_buffer(cameraBuffer, &cameraData);
}

void Camera::destroy_camera_storage_buffer(const VmaAllocator &allocator)
{
    utils::destroy_buffer(allocator, cameraBuffer);
}
