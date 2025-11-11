#include "gm/physics/RigidBodyComponent.hpp"

#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/physics/PhysicsWorld.hpp"
#include "gm/core/Logger.hpp"

namespace {

const char* ToString(gm::physics::RigidBodyComponent::BodyType type) {
    switch (type) {
        case gm::physics::RigidBodyComponent::BodyType::Static: return "Static";
        case gm::physics::RigidBodyComponent::BodyType::Dynamic: return "Dynamic";
    }
    return "Unknown";
}

const char* ToString(gm::physics::RigidBodyComponent::ColliderShape shape) {
    switch (shape) {
        case gm::physics::RigidBodyComponent::ColliderShape::Plane: return "Plane";
        case gm::physics::RigidBodyComponent::ColliderShape::Box: return "Box";
    }
    return "Unknown";
}

} // namespace

namespace gm::physics {

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
    if (m_bodyCreated || !GetOwner()) {
        return;
    }

    auto& physics = PhysicsWorld::Instance();
    if (!physics.IsInitialized()) {
        return;
    }

    auto* owner = GetOwner();
    if (m_bodyType == BodyType::Static && m_colliderShape == ColliderShape::Plane) {
        m_bodyHandle = physics.CreateStaticPlane(*owner, m_planeNormal, m_planeConstant);
    } else if (m_bodyType == BodyType::Dynamic && m_colliderShape == ColliderShape::Box) {
        m_bodyHandle = physics.CreateDynamicBox(*owner, m_boxHalfExtent, m_mass);
    } else {
        const char* ownerName = owner ? owner->GetName().c_str() : "<null>";
        core::Logger::Warning(
            "[RigidBodyComponent] Unsupported body/collider combination (body=%s, collider=%s) on '%s'",
            ToString(m_bodyType),
            ToString(m_colliderShape),
            ownerName);
        return;
    }

    m_bodyCreated = m_bodyHandle.IsValid();
}

void RigidBodyComponent::DestroyPhysicsBody() {
    if (!m_bodyCreated || !m_bodyHandle.IsValid()) {
        return;
    }

    auto& physics = PhysicsWorld::Instance();
    if (physics.IsInitialized()) {
        // Use batched removal for better performance when many bodies are destroyed
        physics.QueueBodyRemoval(m_bodyHandle);
    }

    m_bodyHandle = PhysicsWorld::BodyHandle{};
    m_bodyCreated = false;
}

} // namespace gm::physics

