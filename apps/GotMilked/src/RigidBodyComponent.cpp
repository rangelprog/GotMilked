#include "RigidBodyComponent.hpp"

#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/physics/PhysicsWorld.hpp"

RigidBodyComponent::RigidBodyComponent() = default;

RigidBodyComponent::~RigidBodyComponent() {
    DestroyPhysicsBody();
}

void RigidBodyComponent::Init() {
    if (!m_bodyCreated) {
        CreatePhysicsBody();
    }
}

void RigidBodyComponent::OnDestroy() {
    DestroyPhysicsBody();
}

void RigidBodyComponent::CreatePhysicsBody() {
    if (m_bodyCreated || !owner) {
        return;
    }

    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (!physics.IsInitialized()) {
        return;
    }

    if (m_bodyType == BodyType::Static && m_colliderShape == ColliderShape::Plane) {
        m_bodyHandle = physics.CreateStaticPlane(*owner, m_planeNormal, m_planeConstant);
    } else if (m_bodyType == BodyType::Dynamic && m_colliderShape == ColliderShape::Box) {
        m_bodyHandle = physics.CreateDynamicBox(*owner, m_boxHalfExtent, m_mass);
    } else {
        // Unsupported combination
        return;
    }

    m_bodyCreated = m_bodyHandle.IsValid();
}

void RigidBodyComponent::DestroyPhysicsBody() {
    if (!m_bodyCreated || !m_bodyHandle.IsValid()) {
        return;
    }

    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (physics.IsInitialized()) {
        physics.RemoveBody(m_bodyHandle);
    }

    m_bodyHandle = gm::physics::PhysicsWorld::BodyHandle{};
    m_bodyCreated = false;
}

