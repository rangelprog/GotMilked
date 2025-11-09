#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Rotation-Order: Z (roll) -> Y (yaw) -> X (pitch)
// Das ist eine gängige und stabile Reihenfolge für einfache Szenen.
// Winkel sind in Grad.
struct Transform {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 rotationDeg{0.0f, 0.0f, 0.0f}; // (pitchX, yawY, rollZ) in Grad
  glm::vec3 scale{1.0f, 1.0f, 1.0f};

  glm::mat4 toMat4() const {
    glm::mat4 m(1.0f);
    m = glm::translate(m, position);
    // Reihenfolge: Rz * Ry * Rx
    m = glm::rotate(m, glm::radians(rotationDeg.z), glm::vec3(0, 0, 1)); // roll (Z)
    m = glm::rotate(m, glm::radians(rotationDeg.y), glm::vec3(0, 1, 0)); // yaw  (Y)
    m = glm::rotate(m, glm::radians(rotationDeg.x), glm::vec3(1, 0, 0)); // pitch(X)
    m = glm::scale(m, scale);
    return m;
  }
};
