#include "camera.hpp"

void Camera::setPosition(const glm::vec3 &position) {}

void Camera::setOrientation(const glm::vec3 &orientation) {}

void Camera::setViewMatrix()
{
    glm::mat4 viewRotMat = glm::toMat4(rotationQuat);
    viewMatrix = glm::translate(viewRotMat, position);
}

void Camera::setProjMatrix(
    const float &fov, const float &w, const float &h, const float &near, const float &far)
{
    projMatrix = glm::perspective(fov, w / h, near, far);
}

void Camera::forward(const float &dx)
{
    // glm::vec3 backDir = glm::cross(orientation, glm::vec3(0, 0, 1));
    position += dx * orientation;
}

void Camera::backwards(const float &dx)
{
    // glm::vec3 backDir = glm::cross(orientation, glm::vec3(0, 0, 1));
    position -= dx * orientation;
}

void Camera::right(const float &dx)
{
    // glm::vec3 forwardDir = glm::rotate(orientationQuat, glm::vec3(0, 0, -1));
    // glm::vec3 forwardDir = orientationQuat * glm::vec3(0, 0, -1);
    glm::vec3 leftDir = glm::cross(orientation, glm::vec3(0, 1, 0));
    position -= dx * leftDir;
}

void Camera::left(const float &dx)
{
    // glm::vec3 forwardDir = orientationQuat * glm::vec3(0, 0, -1);
    // glm::vec3 forwardDir = glm::rotate(orientationQuat, glm::vec3(0, 0, -1));
    glm::vec3 leftDir = glm::cross(orientation, glm::vec3(0, 1, 0));
    position += dx * leftDir;
}

void Camera::up(const float &dx)
{
    position += glm::vec3(0, dx, 0);
}

void Camera::down(const float &dx)
{
    position -= glm::vec3(0, dx, 0);
}

void Camera::lookUp(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::vec3(-1, 0, 0));
    rotationQuat *= dr;
    orientation = dr * orientation;
}

void Camera::lookDown(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::vec3(1, 0, 0));
    rotationQuat *= dr;
    orientation = dr * orientation;
}

void Camera::lookRight(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::vec3(0, -1, 0));
    rotationQuat *= dr;
    orientation = dr * orientation;
}

void Camera::lookLeft(const float &dx)
{
    glm::quat dr = glm::angleAxis(dx, glm::vec3(0, 1, 0));
    rotationQuat *= dr;
    orientation = dr * orientation;
}
