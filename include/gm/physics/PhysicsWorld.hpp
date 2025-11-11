#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <glm/vec3.hpp>
#include <vector>
#include <memory>

namespace gm {
class GameObject;
}

namespace gm::physics {

/**
 * @brief Thin wrapper around Jolt's PhysicsSystem.
 *
 * Handles initialization, stepping, and basic body creation helpers for
 * static planes and dynamic boxes. Dynamic bodies automatically drive their
 * owning GameObject's TransformComponent when the simulation advances.
 */
class PhysicsWorld {
public:
    struct BodyHandle {
        JPH::BodyID id = JPH::BodyID();
        bool IsValid() const { return !id.IsInvalid(); }
    };

    struct BodyStats {
        int staticBodies = 0;
        int dynamicBodies = 0;
        int activeDynamicBodies = 0;
        int sleepingDynamicBodies = 0;
    };

    static PhysicsWorld& Instance();

    void Init(const glm::vec3& gravity = glm::vec3(0.0f, -9.81f, 0.0f));
    void Shutdown();
    bool IsInitialized() const { return m_initialized; }

    void Step(float deltaTime);

    BodyHandle CreateStaticPlane(gm::GameObject& object,
                                 const glm::vec3& normal,
                                 float constant);

    BodyHandle CreateDynamicBox(gm::GameObject& object,
                                const glm::vec3& halfExtent,
                                float mass);

    void RemoveBody(const BodyHandle& handle);

    BodyStats GetBodyStats() const;

private:
    PhysicsWorld() = default;
    ~PhysicsWorld() = default;

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    struct DynamicBodyRecord {
        JPH::BodyID id;
        gm::GameObject* gameObject = nullptr;
    };

    void DestroyAllBodies();

    bool m_initialized = false;

    std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_jobSystem;
    std::unique_ptr<JPH::Factory> m_factory;

    std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;

    class BroadPhaseLayerInterfaceImpl;
    class ObjectLayerPairFilterImpl;
    class ObjectVsBroadPhaseLayerFilterImpl;

    std::unique_ptr<BroadPhaseLayerInterfaceImpl> m_broadPhaseLayerInterface;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_objectLayerPairFilter;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objectVsBroadPhaseLayerFilter;

    std::vector<JPH::BodyID> m_staticBodies;
    std::vector<DynamicBodyRecord> m_dynamicBodies;
};

} // namespace gm::physics


