#include "SceneSerializerExtensions.hpp"
#include "MeshSpinnerComponent.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/Component.hpp"
#include "gm/scene/GameObject.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

// Helper to serialize MeshSpinnerComponent
json SerializeMeshSpinnerComponent(MeshSpinnerComponent* comp) {
    json data;
    data["rotationSpeed"] = comp->GetRotationSpeed();
    if (!comp->GetMeshGuid().empty()) {
        data["meshGuid"] = comp->GetMeshGuid();
    }
    data["meshPath"] = comp->GetMeshPath();
    if (!comp->GetTextureGuid().empty()) {
        data["textureGuid"] = comp->GetTextureGuid();
    }
    data["texturePath"] = comp->GetTexturePath();
    if (!comp->GetShaderGuid().empty()) {
        data["shaderGuid"] = comp->GetShaderGuid();
    }
    data["shaderVertPath"] = comp->GetShaderVertPath();
    data["shaderFragPath"] = comp->GetShaderFragPath();
    return data;
}

// Helper to deserialize MeshSpinnerComponent
gm::Component* DeserializeMeshSpinnerComponent(gm::GameObject* obj, const json& data) {
    auto comp = obj->AddComponent<MeshSpinnerComponent>();
    if (!comp) return nullptr;
    
    if (data.contains("rotationSpeed") && data["rotationSpeed"].is_number()) {
        comp->SetRotationSpeed(data["rotationSpeed"].get<float>());
    }

    if (data.contains("meshGuid") && data["meshGuid"].is_string()) {
        comp->SetMeshGuid(data["meshGuid"].get<std::string>());
    }
    if (data.contains("meshPath") && data["meshPath"].is_string()) {
        comp->SetMeshPath(data["meshPath"].get<std::string>());
    }
    if (data.contains("textureGuid") && data["textureGuid"].is_string()) {
        comp->SetTextureGuid(data["textureGuid"].get<std::string>());
    }
    if (data.contains("texturePath") && data["texturePath"].is_string()) {
        comp->SetTexturePath(data["texturePath"].get<std::string>());
    }
    if (data.contains("shaderVertPath") && data["shaderVertPath"].is_string() &&
        data.contains("shaderFragPath") && data["shaderFragPath"].is_string()) {
        comp->SetShaderPaths(
            data["shaderVertPath"].get<std::string>(),
            data["shaderFragPath"].get<std::string>()
        );
    }
    if (data.contains("shaderGuid") && data["shaderGuid"].is_string()) {
        comp->SetShaderGuid(data["shaderGuid"].get<std::string>());
    }
    
    return comp.get();
}

} // anonymous namespace

namespace gm {
namespace SceneSerializerExtensions {

void RegisterSerializers() {
    static bool registered = false;
    if (registered) {
        return;
    }

    auto serialize = [](gm::Component* component) -> json {
        if (auto* spinner = dynamic_cast<MeshSpinnerComponent*>(component)) {
            return SerializeMeshSpinnerComponent(spinner);
        }
        return json();
    };

    auto deserialize = [](gm::GameObject* obj, const json& data) -> gm::Component* {
        return DeserializeMeshSpinnerComponent(obj, data);
    };

    constexpr const char* typeAliases[] = {
        "MeshSpinnerComponent",
        "CowRenderer",
        "CowRendererComponent"
    };

    for (const char* typeName : typeAliases) {
        gm::SceneSerializer::RegisterComponentSerializer(typeName, serialize, deserialize);
    }

    registered = true;
}

void UnregisterSerializers() {
    constexpr const char* typeAliases[] = {
        "MeshSpinnerComponent",
        "CowRenderer",
        "CowRendererComponent"
    };

    for (const char* typeName : typeAliases) {
        gm::SceneSerializer::UnregisterComponentSerializer(typeName);
    }
}

} // namespace SceneSerializerExtensions
} // namespace gm

