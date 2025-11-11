#pragma once

#include "gm/scene/Component.hpp"
#include "gm/physics/PhysicsWorld.hpp"

#include <glm/vec3.hpp>

namespace gm::physics {

/**
 * @brief Component for adding physics simulation to GameObjects
 */
class RigidBodyComponent : public gm::Component {
public:
    enum class BodyType {
        Static,
        Dynamic
    };

    enum class ColliderShape {
        Plane,
        Box
    };

    RigidBodyComponent();
    ~RigidBodyComponent() override;

    void Init() override;
    void OnDestroy() override;

    // Configuration
    void SetBodyType(BodyType type) { m_bodyType = type; }
    BodyType GetBodyType() const { return m_bodyType; }

    void SetColliderShape(ColliderShape shape) { m_colliderShape = shape; }
    ColliderShape GetColliderShape() const { return m_colliderShape; }

    // For plane collider
    void SetPlaneNormal(const glm::vec3& normal) { m_planeNormal = normal; }
    glm::vec3 GetPlaneNormal() const { return m_planeNormal; }
    void SetPlaneConstant(float constant) { m_planeConstant = constant; }
    float GetPlaneConstant() const { return m_planeConstant; }

    // For box collider
    void SetBoxHalfExtent(const glm::vec3& halfExtent) { m_boxHalfExtent = halfExtent; }
    glm::vec3 GetBoxHalfExtent() const { return m_boxHalfExtent; }

    // For dynamic bodies
    void SetMass(float mass) { m_mass = mass; }
    float GetMass() const { return m_mass; }

    // Body handle access
    bool IsValid() const { return m_bodyHandle.IsValid(); }
    PhysicsWorld::BodyHandle GetBodyHandle() const { return m_bodyHandle; }

private:
    void CreatePhysicsBody();
    void DestroyPhysicsBody();

    BodyType m_bodyType = BodyType::Dynamic;
    ColliderShape m_colliderShape = ColliderShape::Box;

    // Plane parameters
    glm::vec3 m_planeNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    float m_planeConstant = 0.0f;

    // Box parameters
    glm::vec3 m_boxHalfExtent = glm::vec3(0.5f);

    // Dynamic body parameters
    float m_mass = 1.0f;

    PhysicsWorld::BodyHandle m_bodyHandle;
    bool m_bodyCreated = false;
};

} // namespace gm::physics

