#include "camera.hpp"

void Camera::setPosition(const glm::vec3 &position) {}

void Camera::setOrientation(const glm::vec3 &orientation) {}

void Camera::setViewMatrix()
{
    glm::mat4 viewRotMat = glm::toMat4(orientationQuat);
    viewMatrix = glm::translate(viewRotMat, position);
}

void Camera::setProjMatrix(
    const float &fov, const float &w, const float &h, const float &near, const float &far)
{
    projMatrix = glm::perspective(fov, w / h, near, far);
}

void Camera::forward(const float &dx)
{
    glm::vec3 backDir = glm::cross(orientationQuat, glm::vec3(0, 0, 1));
    position -= dx * backDir;
}

void Camera::backwards(const float &dx)
{
    glm::vec3 backDir = glm::cross(orientationQuat, glm::vec3(0, 0, 1));
    position += dx * backDir;
}

void Camera::right(const float &dx)
{
    // glm::vec3 forwardDir = glm::rotate(orientationQuat, glm::vec3(0, 0, -1));
    // glm::vec3 forwardDir = orientationQuat * glm::vec3(0, 0, -1);
    glm::vec3 leftDir = glm::cross(orientationQuat, glm::vec3(1, 0, 0));
    position -= dx * leftDir;
}

void Camera::left(const float &dx)
{
    // glm::vec3 forwardDir = orientationQuat * glm::vec3(0, 0, -1);
    // glm::vec3 forwardDir = glm::rotate(orientationQuat, glm::vec3(0, 0, -1));
    glm::vec3 leftDir = glm::cross(orientationQuat, glm::vec3(1, 0, 0));
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
    orientationQuat *= glm::angleAxis(dx, glm::vec3(-1, 0, 0));
}

void Camera::lookDown(const float &dx)
{
    orientationQuat *= glm::angleAxis(dx, glm::vec3(1, 0, 0));
}

void Camera::lookRight(const float &dx)
{
    orientationQuat *= glm::angleAxis(dx, glm::vec3(0, -1, 0));
}

void Camera::lookLeft(const float &dx)
{
    orientationQuat *= glm::angleAxis(dx, glm::vec3(0, 1, 0));
}
