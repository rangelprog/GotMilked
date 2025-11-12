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
