#include "model.hpp"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

void Model::updateModelMatrix()
{
    // The order is translate -> rotate -> scale.
    glm::mat4 transMat = glm::translate(glm::mat4(1.f), position);
    // I implement the rotations as quaternions and I accumulate them in quaternion space
    glm::quat pQuat = glm::angleAxis(pitch, glm::vec3(1, 0, 0));
    glm::quat yQuat = glm::angleAxis(yaw, glm::vec3(0, 1, 0));
    glm::quat rQuat = glm::angleAxis(roll, glm::vec3(0, 0, 1));
    // After that I generate the rotation matrix with them. Would be more efficient to send directly
    // the quaternions + the translations to the shader?
    glm::mat4 rotMat = glm::toMat4(pQuat * yQuat * rQuat);

    glm::mat4 scaleMat = glm::scale(scale);

    // This becomes modelMat = T * R
    modelMatrix = transMat * rotMat * scaleMat;
}
