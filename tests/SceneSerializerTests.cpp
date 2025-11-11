#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Component.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#include <cassert>
#include <cmath>
#include <iostream>

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

void SceneSerializerRoundTrip() {
    gm::SceneSerializer::ClearComponentSerializers();
    RegisterTestSerializer();

    gm::Scene scene("TestScene");
    auto original = scene.CreateGameObject("TestObject");
    auto transform = original->EnsureTransform();
    transform->SetPosition(1.0f, 2.0f, 3.0f);
    transform->SetRotation(10.0f, 20.0f, 30.0f);
    transform->SetScale(2.0f);

    auto testComponent = original->AddComponent<TestComponent>();
    testComponent->SetValue(42.0f);

    std::string serialized = gm::SceneSerializer::Serialize(scene);

    gm::Scene restored("RestoredScene");
    bool deserialized = gm::SceneSerializer::Deserialize(restored, serialized);
    assert(deserialized);

    auto rehydrated = restored.FindGameObjectByName("TestObject");
    assert(rehydrated);
    assert(rehydrated->IsActive());

    auto restoredTransform = rehydrated->GetTransform();
    assert(restoredTransform);

    glm::vec3 restoredPos = restoredTransform->GetPosition();
    assert(restoredPos.x == 1.0f && restoredPos.y == 2.0f && restoredPos.z == 3.0f);

    glm::vec3 restoredRot = restoredTransform->GetRotation();
    assert(restoredRot.x == 10.0f && restoredRot.y == 20.0f && restoredRot.z == 30.0f);

    glm::vec3 restoredScale = restoredTransform->GetScale();
    assert(restoredScale.x == 2.0f && restoredScale.y == 2.0f && restoredScale.z == 2.0f);

    auto restoredTest = rehydrated->GetComponent<TestComponent>();
    assert(restoredTest);
    assert(std::fabs(restoredTest->GetValue() - 42.0f) < 1e-5f);

    gm::SceneSerializer::ClearComponentSerializers();
}

} // namespace

void RunSceneSerializerRoundTripTest() {
    SceneSerializerRoundTrip();
    std::cout << "SceneSerializer round-trip test passed.\n";
}
