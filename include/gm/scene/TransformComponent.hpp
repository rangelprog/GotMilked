#pragma once
#include "Component.hpp"
#include "Transform.hpp"
#include <glm/glm.hpp>

namespace gm {

/**
 * @brief Component that provides transform (position, rotation, scale) for GameObjects
 * 
 * Every GameObject should have a TransformComponent to define its position in the world.
 * This component provides convenient methods for common transform operations.
 */
class TransformComponent : public Component {
private:
    Transform transform;
    mutable bool m_localMatrixDirty = true;
    mutable bool m_worldMatrixDirty = true;
    mutable glm::mat4 m_cachedLocalMatrix{1.0f};
    mutable glm::mat4 m_cachedWorldMatrix{1.0f};

public:
    TransformComponent();
    virtual ~TransformComponent() = default;

    // Position
    glm::vec3 GetPosition() const;
    void SetPosition(const glm::vec3& pos);
    void SetPosition(float x, float y, float z);
    void Translate(const glm::vec3& delta);
    void Translate(float x, float y, float z);

    const glm::vec3& GetLocalPosition() const { return transform.position; }
    void SetLocalPosition(const glm::vec3& pos);

    // Rotation (Euler angles in degrees)
    glm::vec3 GetRotation() const;
    void SetRotation(const glm::vec3& rot);
    void SetRotation(float x, float y, float z);
    void Rotate(const glm::vec3& delta);
    void Rotate(float x, float y, float z);

    const glm::vec3& GetLocalRotation() const { return transform.rotation; }
    void SetLocalRotation(const glm::vec3& rot) { transform.rotation = rot; MarkLocalDirty(); }

    // Scale
    glm::vec3 GetScale() const;
    void SetScale(const glm::vec3& scl);
    void SetScale(float x, float y, float z);
    void SetScale(float uniformScale);
    void Scale(const glm::vec3& delta);
    void Scale(float x, float y, float z);

    const glm::vec3& GetLocalScale() const { return transform.scale; }
    void SetLocalScale(const glm::vec3& scl) { transform.scale = scl; MarkLocalDirty(); }

    // Transform matrix
    const glm::mat4& GetMatrix() const;
    const glm::mat4& GetLocalMatrix() const;
    const Transform& GetTransform() const { return transform; }
    void SetTransform(const Transform& t);

    // Helper methods
    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;

    // Reset to identity
    void Reset();

    void MarkLocalDirty();
    void MarkWorldDirty();
};

} // namespace gm

