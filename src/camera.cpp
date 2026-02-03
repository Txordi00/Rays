#include "camera.hpp"
#include "utils.hpp"
#include <SDL3/SDL.h>
#include <glm/ext.hpp>

void Camera::setProjMatrix(
    const float &fov, const float &w, const float &h, const float &near, const float &far)
{
    projMatrix = glm::perspective(fov, w / h, near, far);
    projInverse = glm::inverse(projMatrix);
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

void Camera::lookAt(const glm::vec3 &point)
{
    orientation = glm::normalize(point - translation);
}

void Camera::update()
{
    orientation = glm::normalize(orientation);
    viewMatrix = glm::lookAt(translation, translation + orientation, glm::vec3(0, 1, 0));
    invView = glm::inverse(viewMatrix);
    if (cameraBuffer.buffer) {
        cameraData.origin = translation;
        cameraData.orientation = orientation;
        cameraData.projInverse = projInverse;
        cameraData.viewInverse = invView;
        utils::copy_to_buffer(cameraBuffer, allocator, &cameraData);
    }
}

void Camera::create_camera_buffer()
{
    cameraBuffer = utils::create_buffer(device,
                                        allocator,
                                        sizeof(CameraData),
                                        vk::BufferUsageFlagBits::eUniformBuffer,
                                        VMA_MEMORY_USAGE_AUTO,
                                        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                            | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    cameraData.origin = translation;
    cameraData.orientation = glm::normalize(orientation);
    cameraData.projInverse = projInverse;
    cameraData.viewInverse = invView;

    utils::copy_to_buffer(cameraBuffer, allocator, &cameraData);
}

void Camera::destroy_camera_buffer()
{
    utils::destroy_buffer(allocator, cameraBuffer);
}

// Camera::Camera()
// {
//     create_dispatch_table();
// }

// void Camera::create_dispatch_table()
// {
//     // Initialize action map in order to avoid many if statements
//     // The 0 function is defined in order to handle undefined key presses or no key press at all
//     actionMap[0] = [](Camera *c, const float dx, const float dtheta) {};
//     actionMap[SDL_SCANCODE_W] = [](Camera *c, const float dx, const float dtheta) {
//         c->forward(dx);
//     };
//     actionMap[SDL_SCANCODE_S] = [](Camera *c, const float dx, const float dtheta) {
//         c->backwards(dx);
//     };
//     actionMap[SDL_SCANCODE_A] = [](Camera *c, const float dx, const float dtheta) { c->left(dx); };
//     actionMap[SDL_SCANCODE_D] = [](Camera *c, const float dx, const float dtheta) { c->right(dx); };
//     actionMap[SDL_SCANCODE_Q] = [](Camera *c, const float dx, const float dtheta) { c->down(dx); };
//     actionMap[SDL_SCANCODE_E] = [](Camera *c, const float dx, const float dtheta) { c->up(dx); };
//     actionMap[SDL_SCANCODE_UP] = [](Camera *c, const float dx, const float dtheta) {
//         c->lookUp(dtheta);
//     };
//     actionMap[SDL_SCANCODE_DOWN] = [](Camera *c, const float dx, const float dtheta) {
//         c->lookDown(dtheta);
//     };
//     actionMap[SDL_SCANCODE_LEFT] = [](Camera *c, const float dx, const float dtheta) {
//         c->lookLeft(dtheta);
//     };
//     actionMap[SDL_SCANCODE_RIGHT] = [](Camera *c, const float dx, const float dtheta) {
//         c->lookRight(dtheta);
//     };
// }

void Camera::process_event(const bool *keyStates, const float dt)
{
    const float dx = std::min(5.f * dt, 0.1f);
    const float dtheta = std::min(2.f * dt, glm::radians(4.f));
    const static std::vector<uint32_t> codes{SDL_SCANCODE_W,
                                             SDL_SCANCODE_S,
                                             SDL_SCANCODE_A,
                                             SDL_SCANCODE_D,
                                             SDL_SCANCODE_Q,
                                             SDL_SCANCODE_E,
                                             SDL_SCANCODE_UP,
                                             SDL_SCANCODE_DOWN,
                                             SDL_SCANCODE_LEFT,
                                             SDL_SCANCODE_RIGHT};

    // I decided to go for a simple switch instead of a dispatch table. Simpler and probably faster
    for (const uint32_t c : codes) {
        uint32_t code = static_cast<uint32_t>(keyStates[c]) * c; // c if pressed, 0 if not
        switch (code) {
        case 0:
            break;
        case SDL_SCANCODE_W:
            forward(dx);
            break;
        case SDL_SCANCODE_S:
            backwards(dx);
            break;
        case SDL_SCANCODE_A:
            left(dx);
            break;
        case SDL_SCANCODE_D:
            right(dx);
            break;
        case SDL_SCANCODE_Q:
            down(dx);
            break;
        case SDL_SCANCODE_E:
            up(dx);
            break;
        case SDL_SCANCODE_UP:
            lookUp(dtheta);
            break;
        case SDL_SCANCODE_DOWN:
            lookDown(dtheta);
            break;
        case SDL_SCANCODE_LEFT:
            lookLeft(dtheta);
            break;
        case SDL_SCANCODE_RIGHT:
            lookRight(dtheta);
            break;
        }
    }
}
