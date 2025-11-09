#include "Camera.hpp"
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

static constexpr glm::vec3 WORLD_UP(0.0f, 1.0f, 0.0f);

Camera::Camera(glm::vec3 position, float yawDeg, float pitchDeg)
    : m_Pos(position), m_Yaw(yawDeg), m_Pitch(pitchDeg) {
  updateBasis();
}

void Camera::moveForward(float d) { m_Pos += m_Front * d; }
void Camera::moveBackward(float d) { m_Pos -= m_Front * d; }
void Camera::moveRight(float d) { m_Pos += m_Right * d; }
void Camera::moveLeft(float d) { m_Pos -= m_Right * d; }
void Camera::moveUp(float d) { m_Pos += m_Up * d; }
void Camera::moveDown(float d) { m_Pos -= m_Up * d; }

void Camera::addYawPitch(float dYawDeg, float dPitchDeg) {
  m_Yaw += dYawDeg;
  m_Pitch += dPitchDeg;
  if (m_Pitch > 89.0f)
    m_Pitch = 89.0f;
  if (m_Pitch < -89.0f)
    m_Pitch = -89.0f;
  updateBasis();
}

glm::mat4 Camera::view() const {
  return glm::lookAt(m_Pos, m_Pos + m_Front, m_Up);
}

void Camera::updateBasis() {
  const float yawR = glm::radians(m_Yaw);
  const float pitchR = glm::radians(m_Pitch);
  glm::vec3 f;
  f.x = std::cos(yawR) * std::cos(pitchR);
  f.y = std::sin(pitchR);
  f.z = std::sin(yawR) * std::cos(pitchR);
  m_Front = glm::normalize(f);
  m_Right = glm::normalize(glm::cross(m_Front, WORLD_UP));
  m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}
