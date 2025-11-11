#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gm {

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f));

    glm::mat4 View() const;
    
    // Movement
    void MoveForward(float deltaTime) { position += front * (movementSpeed * deltaTime); }
    void MoveBackward(float deltaTime) { position -= front * (movementSpeed * deltaTime); }
    void MoveLeft(float deltaTime) { position -= right * (movementSpeed * deltaTime); }
    void MoveRight(float deltaTime) { position += right * (movementSpeed * deltaTime); }
    void MoveUp(float deltaTime) { position += worldUp * (movementSpeed * deltaTime); }
    void MoveDown(float deltaTime) { position -= worldUp * (movementSpeed * deltaTime); }

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);

    float GetZoom() const { return zoom; }
    const glm::vec3& Position() const { return position; }
    const glm::vec3& Front() const { return front; }
    
    // Setters for save/load support
    void SetPosition(const glm::vec3& pos) { position = pos; }
    void SetForward(const glm::vec3& forward);
    void SetFov(float fov) { zoom = fov; }

private:
    void updateCameraVectors();

    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Euler Angles
    float yaw;
    float pitch;

    // Camera options
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;
};

}