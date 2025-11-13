#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

namespace gm {
namespace {
constexpr float kEpsilon = 1e-6f;

glm::vec3 ExtractScale(const glm::mat4& matrix) {
    glm::vec3 scale;
    glm::quat orientation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    if (glm::decompose(matrix, scale, orientation, translation, skew, perspective)) {
        return scale;
    }
    return glm::vec3(1.0f);
}

glm::vec3 ExtractTranslation(const glm::mat4& matrix) {
    return glm::vec3(matrix[3]);
}

glm::vec3 ExtractRotationEuler(const glm::mat4& matrix) {
    glm::vec3 scale;
    glm::quat orientation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    if (glm::decompose(matrix, scale, orientation, translation, skew, perspective)) {
        orientation = glm::conjugate(orientation);
        return glm::degrees(glm::eulerAngles(orientation));
    }
    return glm::vec3(0.0f);
}

glm::vec3 SafeDivide(const glm::vec3& numerator, const glm::vec3& denominator) {
    glm::vec3 result = numerator;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(denominator[i]) > kEpsilon) {
            result[i] = numerator[i] / denominator[i];
        } else {
            result[i] = numerator[i];
        }
    }
    return result;
}
} // namespace

TransformComponent::TransformComponent() : Component() {
    name = "Transform";
    transform = Transform{};
    MarkLocalDirty();
}

glm::vec3 TransformComponent::GetPosition() const {
    return ExtractTranslation(GetMatrix());
}

void TransformComponent::SetLocalPosition(const glm::vec3& pos) {
    transform.position = pos;
    MarkLocalDirty();
}

void TransformComponent::SetPosition(const glm::vec3& pos) {
    auto* ownerObj = GetOwner();
    if (ownerObj && ownerObj->HasParent()) {
        if (auto parent = ownerObj->GetParent()) {
            if (auto parentTransform = parent->GetTransform()) {
                glm::mat4 parentMatrix = parentTransform->GetMatrix();
                glm::mat4 parentInverse = glm::inverse(parentMatrix);
                glm::vec4 localPos = parentInverse * glm::vec4(pos, 1.0f);
                transform.position = glm::vec3(localPos);
                MarkLocalDirty();
                return;
            }
        }
    }
    transform.position = pos;
    MarkLocalDirty();
}

void TransformComponent::SetPosition(float x, float y, float z) {
    SetPosition(glm::vec3(x, y, z));
}

void TransformComponent::Translate(const glm::vec3& delta) {
    SetPosition(GetPosition() + delta);
}

void TransformComponent::Translate(float x, float y, float z) {
    Translate(glm::vec3(x, y, z));
}

glm::vec3 TransformComponent::GetRotation() const {
    return ExtractRotationEuler(GetMatrix());
}

void TransformComponent::SetRotation(const glm::vec3& rot) {
    auto* ownerObj = GetOwner();
    glm::quat worldQuat = glm::quat(glm::radians(rot));
    if (ownerObj && ownerObj->HasParent()) {
        if (auto parent = ownerObj->GetParent()) {
            if (auto parentTransform = parent->GetTransform()) {
                glm::quat parentQuat = glm::quat(glm::radians(parentTransform->GetRotation()));
                glm::quat localQuat = glm::inverse(parentQuat) * worldQuat;
                transform.rotation = glm::degrees(glm::eulerAngles(localQuat));
                MarkLocalDirty();
                return;
            }
        }
    }
    transform.rotation = rot;
    MarkLocalDirty();
}

void TransformComponent::SetRotation(float x, float y, float z) {
    SetRotation(glm::vec3(x, y, z));
}

void TransformComponent::Rotate(const glm::vec3& delta) {
    SetRotation(GetRotation() + delta);
}

void TransformComponent::Rotate(float x, float y, float z) {
    Rotate(glm::vec3(x, y, z));
}

glm::vec3 TransformComponent::GetScale() const {
    return ExtractScale(GetMatrix());
}

void TransformComponent::SetScale(const glm::vec3& scl) {
    auto* ownerObj = GetOwner();
    if (ownerObj && ownerObj->HasParent()) {
        if (auto parent = ownerObj->GetParent()) {
            if (auto parentTransform = parent->GetTransform()) {
                glm::vec3 parentScale = parentTransform->GetScale();
                transform.scale = SafeDivide(scl, parentScale);
                MarkLocalDirty();
                return;
            }
        }
    }
    transform.scale = scl;
    MarkLocalDirty();
}

void TransformComponent::SetScale(float x, float y, float z) {
    SetScale(glm::vec3(x, y, z));
}

void TransformComponent::SetScale(float uniformScale) {
    SetScale(glm::vec3(uniformScale));
}

void TransformComponent::Scale(const glm::vec3& delta) {
    SetScale(GetScale() * delta);
}

void TransformComponent::Scale(float x, float y, float z) {
    Scale(glm::vec3(x, y, z));
}

const glm::mat4& TransformComponent::GetLocalMatrix() const {
    if (m_localMatrixDirty) {
        m_cachedLocalMatrix = transform.getMatrix();
        m_localMatrixDirty = false;
    }
    return m_cachedLocalMatrix;
}

const glm::mat4& TransformComponent::GetMatrix() const {
    if (m_worldMatrixDirty) {
        const glm::mat4& local = GetLocalMatrix();
        auto* ownerObj = GetOwner();
        if (ownerObj && ownerObj->HasParent()) {
            if (auto parent = ownerObj->GetParent()) {
                if (auto parentTransform = parent->GetTransform()) {
                    m_cachedWorldMatrix = parentTransform->GetMatrix() * local;
                } else {
                    m_cachedWorldMatrix = local;
                }
            } else {
                m_cachedWorldMatrix = local;
            }
        } else {
            m_cachedWorldMatrix = local;
        }
        m_worldMatrixDirty = false;
    }
    return m_cachedWorldMatrix;
}

void TransformComponent::SetTransform(const Transform& t) {
    transform = t;
    MarkLocalDirty();
}

glm::vec3 TransformComponent::GetForward() const {
    glm::vec3 rotation = GetRotation();
    float yaw = glm::radians(rotation.y);
    float pitch = glm::radians(rotation.x);

    glm::vec3 forward;
    forward.x = std::cos(yaw) * std::cos(pitch);
    forward.y = std::sin(pitch);
    forward.z = std::sin(yaw) * std::cos(pitch);
    return glm::normalize(forward);
}

glm::vec3 TransformComponent::GetRight() const {
    glm::vec3 forward = GetForward();
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    return glm::normalize(glm::cross(worldUp, forward));
}

glm::vec3 TransformComponent::GetUp() const {
    glm::vec3 forward = GetForward();
    glm::vec3 right = GetRight();
    return glm::normalize(glm::cross(right, forward));
}

void TransformComponent::Reset() {
    transform.position = glm::vec3(0.0f);
    transform.rotation = glm::vec3(0.0f);
    transform.scale = glm::vec3(1.0f);
    MarkLocalDirty();
}

void TransformComponent::MarkLocalDirty() {
    m_localMatrixDirty = true;
    MarkWorldDirty();
}

void TransformComponent::MarkWorldDirty() {
    m_worldMatrixDirty = true;
    if (auto* ownerObj = GetOwner()) {
        ownerObj->PropagateTransformDirty();
    }
}

} // namespace gm

