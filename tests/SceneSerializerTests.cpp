#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Component.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#include <cmath>
#include <filesystem>
#include <chrono>

namespace {

class TestComponent : public gm::Component {
public:
    TestComponent() {
        SetName("TestComponent");
    }

    void SetValue(float v) { m_value = v; }
    float GetValue() const { return m_value; }

private:
    float m_value = 0.0f;
};

void RegisterTestSerializer() {
    using json = nlohmann::json;

    gm::SceneSerializer::RegisterComponentSerializer(
        "TestComponent",
        [](gm::Component* component) -> json {
            auto* test = dynamic_cast<TestComponent*>(component);
            if (!test) {
                return json();
            }
            json data;
            data["value"] = test->GetValue();
            return data;
        },
        [](gm::GameObject* obj, const json& data) -> gm::Component* {
            auto component = obj->AddComponent<TestComponent>();
            if (!component) {
                return nullptr;
            }
            if (data.contains("value") && data["value"].is_number()) {
                component->SetValue(data["value"].get<float>());
            }
            return component.get();
        });
}

struct SerializerGuard {
    SerializerGuard() { gm::SceneSerializer::ClearComponentSerializers(); }
    ~SerializerGuard() { gm::SceneSerializer::ClearComponentSerializers(); }
};

} // namespace

TEST_CASE("SceneSerializer round-trips custom components", "[scene][serialization]") {
    using Catch::Approx;

    SerializerGuard guard;
    RegisterTestSerializer();

    gm::Scene scene("TestScene");
    auto original = scene.CreateGameObject("TestObject");
    auto transform = original->EnsureTransform();
    transform->SetPosition(1.0f, 2.0f, 3.0f);
    transform->SetRotation(10.0f, 20.0f, 30.0f);
    transform->SetScale(2.0f);

    auto testComponent = original->AddComponent<TestComponent>();
    REQUIRE(testComponent);
    testComponent->SetValue(42.0f);

    const std::string serialized = gm::SceneSerializer::Serialize(scene);

    gm::Scene restored("RestoredScene");
    const bool deserialized = gm::SceneSerializer::Deserialize(restored, serialized);
    REQUIRE(deserialized);

    auto rehydrated = restored.FindGameObjectByName("TestObject");
    REQUIRE(rehydrated);
    REQUIRE(rehydrated->IsActive());

    auto restoredTransform = rehydrated->GetTransform();
    REQUIRE(restoredTransform);

    const glm::vec3 restoredPos = restoredTransform->GetPosition();
    REQUIRE(restoredPos.x == Approx(1.0f));
    REQUIRE(restoredPos.y == Approx(2.0f));
    REQUIRE(restoredPos.z == Approx(3.0f));

    const glm::vec3 restoredRot = restoredTransform->GetRotation();
    REQUIRE(restoredRot.x == Approx(10.0f));
    REQUIRE(restoredRot.y == Approx(20.0f));
    REQUIRE(restoredRot.z == Approx(30.0f));

    const glm::vec3 restoredScale = restoredTransform->GetScale();
    REQUIRE(restoredScale.x == Approx(2.0f));
    REQUIRE(restoredScale.y == Approx(2.0f));
    REQUIRE(restoredScale.z == Approx(2.0f));

    auto restoredTest = rehydrated->GetComponent<TestComponent>();
    REQUIRE(restoredTest);
    REQUIRE(restoredTest->GetValue() == Approx(42.0f));
}

TEST_CASE("SceneSerializer streams large scenes without data loss", "[scene][serialization][stream]") {
    SerializerGuard guard;

    gm::Scene scene("LargeScene");
    constexpr int kObjectCount = 500;
    for (int i = 0; i < kObjectCount; ++i) {
        auto obj = scene.CreateGameObject("LargeObject_" + std::to_string(i));
        auto transform = obj->EnsureTransform();
        transform->SetPosition(static_cast<float>(i),
                               static_cast<float>(i * 2),
                               static_cast<float>(i * -3));
        transform->SetScale(1.0f + static_cast<float>(i % 5));
    }

    const auto tempDir = std::filesystem::temp_directory_path();
    const auto uniqueId = static_cast<long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path scenePath = tempDir / ("gm_scene_stream_test_" + std::to_string(uniqueId) + ".json");

    REQUIRE(gm::SceneSerializer::SaveToFile(scene, scenePath.string()));

    gm::Scene restored("LargeSceneRestored");
    REQUIRE(gm::SceneSerializer::LoadFromFile(restored, scenePath.string()));

    const auto& restoredObjects = restored.GetAllGameObjects();
    REQUIRE(restoredObjects.size() == scene.GetAllGameObjects().size());

    auto validateObject = [&](int index) {
        auto restoredObj = restored.FindGameObjectByName("LargeObject_" + std::to_string(index));
        REQUIRE(restoredObj);
        auto transform = restoredObj->GetTransform();
        REQUIRE(transform);
        auto pos = transform->GetPosition();
        REQUIRE(pos.x == Catch::Approx(static_cast<float>(index)));
        REQUIRE(pos.y == Catch::Approx(static_cast<float>(index * 2)));
        REQUIRE(pos.z == Catch::Approx(static_cast<float>(index * -3)));
        auto scale = transform->GetScale();
        REQUIRE(scale.x == Catch::Approx(1.0f + static_cast<float>(index % 5)));
    };

    validateObject(0);
    validateObject(kObjectCount / 2);
    validateObject(kObjectCount - 1);

    std::error_code ec;
    std::filesystem::remove(scenePath, ec);
}

TEST_CASE("SceneSerializer rejects malformed JSON", "[scene][serialization]") {
    gm::Scene scene("MalformedTest");
    const std::string invalidJson = "{ this is not valid json";
    REQUIRE_FALSE(gm::SceneSerializer::Deserialize(scene, invalidJson));
}
