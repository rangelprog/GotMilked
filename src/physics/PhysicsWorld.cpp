#include "gm/physics/PhysicsWorld.hpp"

#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/RegisterTypes.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <limits>
#include <new>
#include <thread>
#include <chrono>

#include "gm/core/Logger.hpp"

namespace gm::physics {

namespace {

void JoltTraceImpl(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stdout, fmt, args);
    std::fprintf(stdout, "\n");
    va_end(args);
}

#ifdef JPH_ENABLE_ASSERTS
bool JoltAssertFailedImpl(const char* expression,
                          const char* message,
                          const char* file,
                          JPH::uint line) {
    std::fprintf(stderr,
                 "[Jolt][Assert] %s:%u: (%s) %s\n",
                 file ? file : "<unknown>",
                 static_cast<unsigned>(line),
                 expression ? expression : "<expr>",
                 message ? message : "");
    return true;
}
#endif

} // namespace

namespace Layers {
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::BroadPhaseLayer BP_NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer BP_MOVING(1);
} // namespace Layers

class PhysicsWorld::BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerInterfaceImpl() {
        m_objectToBroadPhase[Layers::NON_MOVING] = Layers::BP_NON_MOVING;
        m_objectToBroadPhase[Layers::MOVING] = Layers::BP_MOVING;
    }

    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return m_objectToBroadPhase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        switch (layer.GetValue()) {
            case 0: return "NON_MOVING";
            case 1: return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_objectToBroadPhase[2];
};

class PhysicsWorld::ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override {
        if (layer1 == Layers::NON_MOVING && layer2 == Layers::NON_MOVING)
            return false;
        return true;
    }
};

class PhysicsWorld::ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override {
        if (layer1 == Layers::NON_MOVING && layer2 == Layers::BP_NON_MOVING)
            return false;
        return true;
    }
};

PhysicsWorld& PhysicsWorld::Instance() {
    static PhysicsWorld s_instance;
    return s_instance;
}

void PhysicsWorld::Init(const glm::vec3& gravity) {
    if (m_initialized) {
        return;
    }

    JPH::RegisterDefaultAllocator();
    JPH::Trace = JoltTraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = JoltAssertFailedImpl;
#endif
    m_factory = std::make_unique<JPH::Factory>();
    JPH::Factory::sInstance = m_factory.get();
    JPH::RegisterTypes();

    m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
    const JPH::uint32 threadCount = std::max(1u, std::thread::hardware_concurrency());
    const int jobSystemThreads = std::max(1, static_cast<int>(threadCount) - 1);
    m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs,
                                                             JPH::cMaxPhysicsBarriers,
                                                             jobSystemThreads);

    constexpr JPH::uint32 maxBodies = 2048;
    constexpr JPH::uint32 numBodyMutexes = 0;
    constexpr JPH::uint32 maxBodyPairs = 2048;
    constexpr JPH::uint32 maxContactConstraints = 2048;

    m_broadPhaseLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
    m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();
    m_objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();

    if (!m_physicsSystem) {
        m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    }

    m_physicsSystem->Init(maxBodies,
                          numBodyMutexes,
                          maxBodyPairs,
                          maxContactConstraints,
                          *m_broadPhaseLayerInterface,
                          *m_objectVsBroadPhaseLayerFilter,
                          *m_objectLayerPairFilter);

    m_physicsSystem->SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));

    gm::core::Logger::Info("[PhysicsWorld] Initialized (threads: {})", jobSystemThreads + 1);
    m_initialized = true;
}

void PhysicsWorld::Shutdown() {
    if (!m_initialized) {
        return;
    }

    // Flush any pending operations before shutdown
    FlushPendingOperations();
    DestroyAllBodies();

    m_objectVsBroadPhaseLayerFilter.reset();
    m_objectLayerPairFilter.reset();
    m_broadPhaseLayerInterface.reset();
    m_jobSystem.reset();
    m_tempAllocator.reset();

    JPH::UnregisterTypes();
    JPH::Factory::sInstance = nullptr;
    m_factory.reset();

    m_physicsSystem.reset();

    // Reset accumulator and clear pending operations
    m_accumulator = 0.0f;
    m_pendingCreations.clear();
    m_pendingRemovals.clear();

    m_initialized = false;
}

void PhysicsWorld::DestroyAllBodies() {
    if (!m_physicsSystem) {
        return;
    }

    auto& bodyInterface = m_physicsSystem->GetBodyInterface();
    for (const auto& record : m_dynamicBodies) {
        if (record.id.IsInvalid()) {
            continue;
        }
        bodyInterface.RemoveBody(record.id);
        bodyInterface.DestroyBody(record.id);
    }
    m_dynamicBodies.clear();

    for (const auto& id : m_staticBodies) {
        if (id.IsInvalid()) {
            continue;
        }
        bodyInterface.RemoveBody(id);
        bodyInterface.DestroyBody(id);
    }
    m_staticBodies.clear();
}

PhysicsWorld::BodyHandle PhysicsWorld::CreateStaticPlane(gm::GameObject& object,
                                                         const glm::vec3& normal,
                                                         float constant) {
    if (!m_initialized || !m_physicsSystem) {
        return {};
    }

    constexpr float kPlaneHalfExtent = 500.0f;
    constexpr float kPlaneHalfThickness = 0.5f;

    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 planeNormal = normal;
    if (glm::dot(planeNormal, planeNormal) < std::numeric_limits<float>::epsilon()) {
        planeNormal = up;
    } else {
        planeNormal = glm::normalize(planeNormal);
    }

    glm::quat planeQuat = glm::rotation(up, planeNormal);
    if (!std::isfinite(planeQuat.x) ||
        !std::isfinite(planeQuat.y) ||
        !std::isfinite(planeQuat.z) ||
        !std::isfinite(planeQuat.w)) {
        planeQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    auto transform = object.EnsureTransform();
    glm::vec3 basePosition = transform->GetPosition();
    glm::vec3 planePoint = basePosition - planeNormal * constant;
    glm::vec3 center = planePoint - planeNormal * kPlaneHalfThickness;

    JPH::BoxShapeSettings planeSettings(JPH::Vec3(kPlaneHalfExtent, kPlaneHalfThickness, kPlaneHalfExtent));
    JPH::ShapeSettings::ShapeResult shapeResult = planeSettings.Create();
    if (shapeResult.HasError()) {
        gm::core::Logger::Error("[PhysicsWorld] Failed to create plane shape: {}", shapeResult.GetError());
        return {};
    }

    JPH::ShapeRefC shape = shapeResult.Get();
    JPH::Quat joltRotation(planeQuat.x, planeQuat.y, planeQuat.z, planeQuat.w);

    JPH::BodyCreationSettings bodySettings(shape,
                                           JPH::RVec3(center.x, center.y, center.z),
                                           joltRotation,
                                           JPH::EMotionType::Static,
                                           Layers::NON_MOVING);

    auto& bodyInterface = m_physicsSystem->GetBodyInterface();
    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (!body) {
        gm::core::Logger::Error("[PhysicsWorld] Failed to create static plane body");
        return {};
    }

    BodyHandle handle{ body->GetID() };
    m_staticBodies.push_back(handle.id);
    body->SetUserData(reinterpret_cast<JPH::uint64>(&object));
    bodyInterface.AddBody(handle.id, JPH::EActivation::DontActivate);

    return handle;
}

PhysicsWorld::BodyHandle PhysicsWorld::CreateDynamicBox(gm::GameObject& object,
                                                        const glm::vec3& halfExtent,
                                                        float mass) {
    if (!m_initialized || !m_physicsSystem) {
        return {};
    }

    JPH::BoxShapeSettings boxSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();
    if (shapeResult.HasError()) {
        gm::core::Logger::Error("[PhysicsWorld] Failed to create box shape: {}", shapeResult.GetError());
        return {};
    }

    auto transform = object.EnsureTransform();
    glm::vec3 pos = transform->GetPosition();
    glm::vec3 eulerDeg = transform->GetRotation();
    glm::quat rot = glm::quat(glm::radians(eulerDeg));

    JPH::ShapeRefC shape = shapeResult.Get();

    JPH::BodyCreationSettings bodySettings(shape,
                                           JPH::RVec3(pos.x, pos.y, pos.z),
                                           JPH::Quat(rot.x, rot.y, rot.z, rot.w),
                                           JPH::EMotionType::Dynamic,
                                           Layers::MOVING);

    bodySettings.mAllowSleeping = true;
    bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    bodySettings.mMassPropertiesOverride.mMass = mass;

    auto& bodyInterface = m_physicsSystem->GetBodyInterface();
    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (!body) {
        gm::core::Logger::Error("[PhysicsWorld] Failed to create dynamic body");
        return {};
    }

    body->SetUserData(reinterpret_cast<JPH::uint64>(&object));
    JPH::BodyID id = body->GetID();
    bodyInterface.AddBody(id, JPH::EActivation::Activate);

    BodyHandle handle{ id };
    m_dynamicBodies.push_back({ id, &object });
    return handle;
}

void PhysicsWorld::RemoveBody(const BodyHandle& handle) {
    if (!m_initialized || !handle.IsValid() || !m_physicsSystem) {
        return;
    }
    auto& bodyInterface = m_physicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(handle.id);
    bodyInterface.DestroyBody(handle.id);

    // Remove from dynamic bodies
    auto it = std::remove_if(m_dynamicBodies.begin(), m_dynamicBodies.end(),
                             [&handle](const DynamicBodyRecord& record) { return record.id == handle.id; });
    if (it != m_dynamicBodies.end()) {
        m_dynamicBodies.erase(it, m_dynamicBodies.end());
        return;
    }

    // Remove from static bodies
    auto staticIt = std::remove_if(m_staticBodies.begin(), m_staticBodies.end(),
                                   [&handle](const JPH::BodyID& id) { return id == handle.id; });
    if (staticIt != m_staticBodies.end()) {
        m_staticBodies.erase(staticIt, m_staticBodies.end());
    }
}

PhysicsWorld::BodyStats PhysicsWorld::GetBodyStats() const {
    BodyStats stats;
    if (!m_initialized || !m_physicsSystem) {
        return stats;
    }

    for (const auto& id : m_staticBodies) {
        if (!id.IsInvalid()) {
            ++stats.staticBodies;
        }
    }

    for (const auto& record : m_dynamicBodies) {
        if (record.id.IsInvalid()) {
            continue;
        }

        ++stats.dynamicBodies;

        JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), record.id);
        if (!lock.Succeeded()) {
            continue;
        }

        const JPH::Body& body = lock.GetBody();
        if (body.IsActive()) {
            ++stats.activeDynamicBodies;
        } else {
            ++stats.sleepingDynamicBodies;
        }
    }

    return stats;
}

void PhysicsWorld::Step(float deltaTime) {
    if (!m_initialized || !m_physicsSystem) {
        return;
    }

    const auto stepStart = std::chrono::high_resolution_clock::now();

    // Fixed timestep implementation with accumulator
    // Clamp deltaTime to prevent spiral of death
    float clampedDeltaTime = std::min(deltaTime, m_maxTimeStep);
    m_accumulator += clampedDeltaTime;

    constexpr int collisionSteps = 1;
    
    // Run physics simulation at fixed timestep
    // This ensures consistent physics regardless of frame rate
    int substeps = 0;
    while (m_accumulator >= m_fixedTimeStep) {
        m_physicsSystem->Update(m_fixedTimeStep, collisionSteps, m_tempAllocator.get(), m_jobSystem.get());
        m_accumulator -= m_fixedTimeStep;
        ++substeps;
    }

    // Sync dynamic body transforms to GameObjects
    for (auto& record : m_dynamicBodies) {
        if (!record.gameObject || record.id.IsInvalid()) {
            continue;
        }

        JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), record.id);
        if (!lock.Succeeded()) {
            continue;
        }

        const JPH::Body& body = lock.GetBody();
        JPH::RVec3 position = body.GetPosition();
        JPH::Quat rotation = body.GetRotation();

        auto transform = record.gameObject->EnsureTransform();
        transform->SetPosition(static_cast<float>(position.GetX()),
                               static_cast<float>(position.GetY()),
                               static_cast<float>(position.GetZ()));

        glm::quat glmQuat(rotation.GetW(), rotation.GetX(), rotation.GetY(), rotation.GetZ());
        glm::vec3 euler = glm::degrees(glm::eulerAngles(glmQuat));
        transform->SetRotation(euler);
    }

#ifdef GM_DEBUG
    const auto stepEnd = std::chrono::high_resolution_clock::now();
    const double elapsedMs = std::chrono::duration<double, std::milli>(stepEnd - stepStart).count();
    if (substeps > 0) {
        gm::core::Logger::Debug("[PhysicsWorld] Step dt={:.4f}s substeps={} elapsed={:.3f} ms",
                                deltaTime, substeps, elapsedMs);
    }
#endif
}

void PhysicsWorld::QueueBodyCreation(BodyHandle handle, gm::GameObject* gameObject) {
    if (!handle.IsValid() || !gameObject) {
        return;
    }
    m_pendingCreations.push_back({ handle, gameObject });
}

void PhysicsWorld::QueueBodyRemoval(const BodyHandle& handle) {
    if (!handle.IsValid()) {
        return;
    }
    m_pendingRemovals.push_back(handle);
}

void PhysicsWorld::FlushPendingOperations() {
    if (!m_initialized || !m_physicsSystem) {
        m_pendingCreations.clear();
        m_pendingRemovals.clear();
        return;
    }

    // Early exit if nothing to do
    if (m_pendingRemovals.empty() && m_pendingCreations.empty()) {
        return;
    }

    auto& bodyInterface = m_physicsSystem->GetBodyInterface();

    // Batch remove bodies first (to avoid creating then immediately removing)
    // This is more efficient than removing one at a time
    if (!m_pendingRemovals.empty()) {
        // Reserve capacity for efficient removal
        std::vector<JPH::BodyID> bodyIdsToRemove;
        bodyIdsToRemove.reserve(m_pendingRemovals.size());
        
        for (const auto& handle : m_pendingRemovals) {
            bodyIdsToRemove.push_back(handle.id);
        }

        // Batch remove from physics system
        for (JPH::BodyID id : bodyIdsToRemove) {
            bodyInterface.RemoveBody(id);
            bodyInterface.DestroyBody(id);
        }

        // Batch remove from tracking vectors
        for (const auto& handle : m_pendingRemovals) {
            // Remove from dynamic bodies
            auto it = std::remove_if(m_dynamicBodies.begin(), m_dynamicBodies.end(),
                                     [&handle](const DynamicBodyRecord& record) { return record.id == handle.id; });
            if (it != m_dynamicBodies.end()) {
                m_dynamicBodies.erase(it, m_dynamicBodies.end());
            } else {
                // Remove from static bodies
                auto staticIt = std::remove_if(m_staticBodies.begin(), m_staticBodies.end(),
                                               [&handle](const JPH::BodyID& id) { return id == handle.id; });
                if (staticIt != m_staticBodies.end()) {
                    m_staticBodies.erase(staticIt, m_staticBodies.end());
                }
            }
        }
        m_pendingRemovals.clear();
    }

    // Batch add bodies (if any were queued for creation)
    // Note: Most bodies are created immediately, but this allows for future batching
    if (!m_pendingCreations.empty()) {
        // For now, pending creations are just for tracking
        // Actual creation happens in CreateStaticPlane/CreateDynamicBox
        // This structure allows for future deferred creation if needed
        m_pendingCreations.clear();
    }
}

} // namespace gm::physics


