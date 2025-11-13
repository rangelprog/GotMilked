#include "gm/physics/PhysicsWorld.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {

void EnsureShutdown() {
    auto& world = gm::physics::PhysicsWorld::Instance();
    if (world.IsInitialized()) {
        world.Shutdown();
    }
}

} // namespace

TEST_CASE("PhysicsWorld creates and removes dynamic bodies", "[physics][lifecycle]") {
    EnsureShutdown();
    auto& world = gm::physics::PhysicsWorld::Instance();
    world.Init();

    gm::Scene scene("PhysicsScene");
    auto box = scene.CreateGameObject("DynamicBox");
    box->EnsureTransform();

    auto handle = world.CreateDynamicBox(*box, {0.5f, 0.5f, 0.5f}, 2.0f);
    REQUIRE(handle.IsValid());

    auto stats = world.GetBodyStats();
    CHECK(stats.dynamicBodies == 1);

    world.RemoveBody(handle);
    world.FlushPendingOperations();

    stats = world.GetBodyStats();
    CHECK(stats.dynamicBodies == 0);

    world.Shutdown();
}

TEST_CASE("PhysicsWorld queues body removal", "[physics][queues]") {
    EnsureShutdown();
    auto& world = gm::physics::PhysicsWorld::Instance();
    world.Init();

    gm::Scene scene("PhysicsQueueScene");
    auto box = scene.CreateGameObject("QueuedBox");
    box->EnsureTransform();

    auto handle = world.CreateDynamicBox(*box, {1.0f, 1.0f, 1.0f}, 5.0f);
    REQUIRE(handle.IsValid());

    world.QueueBodyRemoval(handle);
    world.FlushPendingOperations();

    auto stats = world.GetBodyStats();
    CHECK(stats.dynamicBodies == 0);

    world.Shutdown();
}
