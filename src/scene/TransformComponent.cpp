#include "gm/scene/TransformComponent.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace gm {

TransformComponent::TransformComponent() : Component() {
    name = "Transform";
    transform = Transform{}; // Initialize with default values
}

void TransformComponent::SetPosition(const glm::vec3& pos) {
    transform.position = pos;
    matrixDirty = true;
}

void TransformComponent::SetPosition(float x, float y, float z) {
    transform.position = glm::vec3(x, y, z);
    matrixDirty = true;
}

void TransformComponent::Translate(const glm::vec3& delta) {
    transform.position += delta;
    matrixDirty = true;
}

void TransformComponent::Translate(float x, float y, float z) {
    transform.position += glm::vec3(x, y, z);
    matrixDirty = true;
}

void TransformComponent::SetRotation(const glm::vec3& rot) {
    transform.rotation = rot;
    matrixDirty = true;
}

void TransformComponent::SetRotation(float x, float y, float z) {
    transform.rotation = glm::vec3(x, y, z);
    matrixDirty = true;
}

void TransformComponent::Rotate(const glm::vec3& delta) {
    transform.rotation += delta;
    matrixDirty = true;
}

void TransformComponent::Rotate(float x, float y, float z) {
    transform.rotation += glm::vec3(x, y, z);
    matrixDirty = true;
}

void TransformComponent::SetScale(const glm::vec3& scl) {
    transform.scale = scl;
    matrixDirty = true;
}

void TransformComponent::SetScale(float x, float y, float z) {
    transform.scale = glm::vec3(x, y, z);
    matrixDirty = true;
}

void TransformComponent::SetScale(float uniformScale) {
    transform.scale = glm::vec3(uniformScale);
    matrixDirty = true;
}

void TransformComponent::Scale(const glm::vec3& delta) {
    transform.scale *= delta;
    matrixDirty = true;
}

void TransformComponent::Scale(float x, float y, float z) {
    transform.scale *= glm::vec3(x, y, z);
    matrixDirty = true;
}

const glm::mat4& TransformComponent::GetMatrix() const {
    if (matrixDirty) {
        cachedMatrix = transform.getMatrix();
        matrixDirty = false;
    }
    return cachedMatrix;
}

void TransformComponent::SetTransform(const Transform& t) {
    transform = t;
    matrixDirty = true;
}

glm::vec3 TransformComponent::GetForward() const {
    // Calculate forward vector from rotation
    float yaw = glm::radians(transform.rotation.y);
    float pitch = glm::radians(transform.rotation.x);
    
    glm::vec3 forward;
    forward.x = cos(yaw) * cos(pitch);
    forward.y = sin(pitch);
    forward.z = sin(yaw) * cos(pitch);
    return glm::normalize(forward);
}

glm::vec3 TransformComponent::GetRight() const {
    // Right is world up cross forward (right-hand rule)
    glm::vec3 forward = GetForward();
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(worldUp, forward));
}

glm::vec3 TransformComponent::GetUp() const {
    // Up is right cross forward
    glm::vec3 forward = GetForward();
    glm::vec3 right = GetRight();
    return glm::normalize(glm::cross(right, forward));
}

void TransformComponent::Reset() {
    transform.position = glm::vec3(0.0f);
    transform.rotation = glm::vec3(0.0f);
    transform.scale = glm::vec3(1.0f);
    matrixDirty = true;
}

} // namespace gm

