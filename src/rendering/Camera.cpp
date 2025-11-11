#include "gm/rendering/Camera.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gm {

Camera::Camera(glm::vec3 pos)
    : position(pos),
      front(glm::vec3(0.0f, 0.0f, -1.0f)),
      worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
      yaw(-90.0f),
      pitch(0.0f),
      movementSpeed(2.5f),
      mouseSensitivity(0.1f),
      zoom(45.0f)
{
    updateCameraVectors();
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch) {
    xoffset *= mouseSensitivity;
    yoffset *= mouseSensitivity;

    yaw   += xoffset;
    pitch += yoffset;

    if (constrainPitch) {
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset) {
    zoom -= yoffset;
    if (zoom < 1.0f)
        zoom = 1.0f;
    if (zoom > 45.0f)
        zoom = 45.0f;
}

glm::mat4 Camera::View() const {
    return glm::lookAt(position, position + front, up);
}

void Camera::SetForward(const glm::vec3& forward) {
    front = glm::normalize(forward);
    
    // Calculate yaw and pitch from forward vector
    // Camera uses: front.x = cos(yaw) * cos(pitch), front.y = sin(pitch), front.z = sin(yaw) * cos(pitch)
    glm::vec3 normalized = glm::normalize(front);
    pitch = glm::degrees(asin(normalized.y));
    
    // Calculate yaw from x-z plane projection
    // Project forward onto x-z plane: (normalized.x, normalized.z)
    float cosPitch = cos(glm::radians(pitch));
    if (cosPitch > 0.001f) { // Avoid division by zero
        // From the camera equations: front.x = cos(yaw) * cos(pitch), front.z = sin(yaw) * cos(pitch)
        // So: cos(yaw) = front.x / cos(pitch), sin(yaw) = front.z / cos(pitch)
        yaw = glm::degrees(atan2(normalized.z / cosPitch, normalized.x / cosPitch));
    }
    // At gimbal lock (pitch near ±90°), yaw is undefined, so we preserve current value
    
    // Constrain pitch
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
    
    updateCameraVectors();
}

void Camera::updateCameraVectors() {
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    right = glm::normalize(glm::cross(front, worldUp));
    up    = glm::normalize(glm::cross(right, front));
}

} // namespace gm
