#pragma once
#include <glm/glm.hpp>

class Camera {
public:
  Camera(glm::vec3 position = {0.0f, 0.0f, 2.0f}, float yawDeg = -90.0f,
         float pitchDeg = 0.0f);

  // movement
  void moveForward(float d);
  void moveBackward(float d);
  void moveRight(float d);
  void moveLeft(float d);
  void moveUp(float d);
  void moveDown(float d);

  // mouse deltas in degrees
  void addYawPitch(float dYawDeg, float dPitchDeg);

  // matrices / vectors
  glm::mat4 view() const;
  glm::vec3 position() const { return m_Pos; }
  glm::vec3 front() const { return m_Front; }
  glm::vec3 right() const { return m_Right; }
  glm::vec3 up() const { return m_Up; }

private:
  void updateBasis();

  glm::vec3 m_Pos;
  float m_Yaw;   // degrees
  float m_Pitch; // degrees

  glm::vec3 m_Front{0.0f, 0.0f, -1.0f};
  glm::vec3 m_Right{1.0f, 0.0f, 0.0f};
  glm::vec3 m_Up{0.0f, 1.0f, 0.0f};
};
