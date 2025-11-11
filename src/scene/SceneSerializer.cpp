#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Component.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/MaterialComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/core/Logger.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>

// Include JSON library
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace gm {
namespace {
    struct CustomSerializerEntry {
        SceneSerializer::SerializeCallback serialize;
        SceneSerializer::DeserializeCallback deserialize;
    };

    static std::unordered_map<std::string, CustomSerializerEntry>& CustomSerializerRegistry() {
        static std::unordered_map<std::string, CustomSerializerEntry> registry;
        return registry;
    }
} // namespace

// Helper: Convert glm::vec3 to JSON array
static json vec3ToJson(const glm::vec3& v) {
    return json::array({v.x, v.y, v.z});
}

// Helper: Convert JSON array to glm::vec3
static glm::vec3 jsonToVec3(const json& j) {
    if (j.is_array() && j.size() >= 3) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }
    return glm::vec3(0.0f);
}

// Helper: Convert glm::mat4 to JSON (store as array of 16 floats)
static json mat4ToJson(const glm::mat4& m) {
    json arr = json::array();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            arr.push_back(m[i][j]);
        }
    }
    return arr;
}

bool SceneSerializer::SaveToFile(Scene& scene, const std::string& filepath) {
    try {
        std::string jsonString = Serialize(scene);
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            core::Logger::Error("[SceneSerializer] Failed to open file for writing: %s",
                                filepath.c_str());
            return false;
        }
        
        file << jsonString;
        file.close();
        
        core::Logger::Debug("[SceneSerializer] Saved scene to: %s", filepath.c_str());
        return true;
    } catch (const std::exception& e) {
        core::Logger::Error("[SceneSerializer] Error saving scene: %s", e.what());
        return false;
    }
}

bool SceneSerializer::LoadFromFile(Scene& scene, const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            core::Logger::Error("[SceneSerializer] Failed to open file for reading: %s",
                                filepath.c_str());
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        return Deserialize(scene, buffer.str());
    } catch (const std::exception& e) {
        core::Logger::Error("[SceneSerializer] Error loading scene: %s", e.what());
        return false;
    }
}

std::string SceneSerializer::Serialize(Scene& scene) {
    json sceneJson;
    
    // Scene metadata
    sceneJson["name"] = scene.GetName();
    sceneJson["isPaused"] = scene.IsPaused();
    
    // Serialize all GameObjects
    json gameObjectsJson = json::array();
    for (const auto& obj : scene.GetAllGameObjects()) {
        if (obj && !obj->IsDestroyed()) {
            gameObjectsJson.push_back(SerializeGameObject(obj));
        }
    }
    sceneJson["gameObjects"] = gameObjectsJson;
    
    // Pretty print with indentation
    return sceneJson.dump(4);
}

bool SceneSerializer::Deserialize(Scene& scene, const std::string& jsonString) {
    try {
        json sceneJson = json::parse(jsonString);
        
        // Clear existing objects before loading
        scene.Cleanup();
        
        // Deserialize GameObjects
        if (sceneJson.contains("gameObjects") && sceneJson["gameObjects"].is_array()) {
            for (const auto& objJson : sceneJson["gameObjects"]) {
                DeserializeGameObject(scene, objJson);
            }
        }
        
        // Set scene properties
        if (sceneJson.contains("name")) {
            // Note: Scene name is set in constructor, we can't change it easily
        }
        if (sceneJson.contains("isPaused")) {
            scene.SetPaused(sceneJson["isPaused"].get<bool>());
        }
        
        core::Logger::Debug("[SceneSerializer] Loaded scene with %zu objects",
                           scene.GetAllGameObjects().size());
        return true;
    } catch (const std::exception& e) {
        core::Logger::Error("[SceneSerializer] Error deserializing scene: %s", e.what());
        return false;
    }
}

void SceneSerializer::RegisterComponentSerializer(const std::string& typeName,
                                                  SerializeCallback serializer,
                                                  DeserializeCallback deserializer) {
    if (typeName.empty() || !serializer || !deserializer) {
        core::Logger::Warning(
            "[SceneSerializer] Invalid custom serializer registration for type '%s'",
            typeName.c_str());
        return;
    }
    CustomSerializerRegistry()[typeName] = {serializer, deserializer};
}

void SceneSerializer::UnregisterComponentSerializer(const std::string& typeName) {
    CustomSerializerRegistry().erase(typeName);
}

void SceneSerializer::ClearComponentSerializers() {
    CustomSerializerRegistry().clear();
}

json SceneSerializer::SerializeGameObject(std::shared_ptr<GameObject> obj) {
    json objJson;
    
    objJson["name"] = obj->GetName();
    objJson["active"] = obj->IsActive();
    
    // Serialize tags
    json tagsJson = json::array();
    for (const auto& tag : obj->GetTags()) {
        tagsJson.push_back(tag);
    }
    objJson["tags"] = tagsJson;
    
    // Serialize components
    json componentsJson = json::array();
    for (const auto& component : obj->GetComponents()) {
        if (component && component->IsActive()) {
            json compJson = SerializeComponent(component.get());
            if (!compJson.is_null()) {
                componentsJson.push_back(compJson);
            }
        }
    }
    objJson["components"] = componentsJson;
    
    return objJson;
}

std::shared_ptr<GameObject> SceneSerializer::DeserializeGameObject(Scene& scene, const json& objectJson) {
    if (!objectJson.contains("name") || !objectJson["name"].is_string()) {
        core::Logger::Warning("[SceneSerializer] GameObject missing name, skipping");
        return nullptr;
    }
    
    std::string name = objectJson["name"].get<std::string>();
    auto obj = scene.CreateGameObject(name);
    
    if (!obj) {
        core::Logger::Warning("[SceneSerializer] Failed to create GameObject: %s",
                              name.c_str());
        return nullptr;
    }
    
    // Set active state
    if (objectJson.contains("active") && objectJson["active"].is_boolean()) {
        obj->SetActive(objectJson["active"].get<bool>());
    }
    
    // Deserialize tags (register with scene for proper tag management)
    if (objectJson.contains("tags") && objectJson["tags"].is_array()) {
        for (const auto& tagJson : objectJson["tags"]) {
            if (tagJson.is_string()) {
                std::string tag = tagJson.get<std::string>();
                obj->AddTag(tag);
                scene.TagGameObject(obj, tag);
            }
        }
    }
    
    // Deserialize components
    if (objectJson.contains("components") && objectJson["components"].is_array()) {
        for (const auto& compJson : objectJson["components"]) {
            DeserializeComponent(obj.get(), compJson);
        }
    }
    
    return obj;
}

json SceneSerializer::SerializeComponent(Component* component) {
    if (!component) return json();
    
    json compJson;
    compJson["type"] = component->GetName();
    compJson["active"] = component->IsActive();
    
    // Component-specific serialization
    std::string type = component->GetName();
    if (type == "TransformComponent" || type == "Transform") {
        json data = SerializeTransformComponent(component);
        compJson["data"] = data;
    } else if (type == "MaterialComponent") {
        json data = SerializeMaterialComponent(component);
        compJson["data"] = data;
    } else if (type == "LightComponent") {
        json data = SerializeLightComponent(component);
        compJson["data"] = data;
    } else {
        // Try registered custom component serializers
        auto& registry = CustomSerializerRegistry();
        auto it = registry.find(type);
        if (it != registry.end() && it->second.serialize) {
            json customData = it->second.serialize(component);
            compJson["data"] = customData;
        } else {
            // Unknown component type - skip
            core::Logger::Warning("[SceneSerializer] Unknown component type: %s (skipping)",
                                  type.c_str());
            return json(); // Return null to skip
        }
    }
    
    return compJson;
}

void SceneSerializer::DeserializeComponent(GameObject* obj, const json& componentJson) {
    if (!componentJson.contains("type") || !componentJson["type"].is_string()) {
        return;
    }
    
    std::string type = componentJson["type"].get<std::string>();
    bool active = true;
    if (componentJson.contains("active") && componentJson["active"].is_boolean()) {
        active = componentJson["active"].get<bool>();
    }
    
    json data;
    if (componentJson.contains("data") && componentJson["data"].is_object()) {
        data = componentJson["data"];
    }
    
    // Component-specific deserialization
    Component* component = nullptr;
    if (type == "TransformComponent" || type == "Transform") {
        component = DeserializeTransformComponent(obj, data);
    } else if (type == "MaterialComponent") {
        component = DeserializeMaterialComponent(obj, data);
    } else if (type == "LightComponent") {
        component = DeserializeLightComponent(obj, data);
    } else {
        // Try registered custom component deserializers
        auto& registry = CustomSerializerRegistry();
        auto it = registry.find(type);
        if (it != registry.end() && it->second.deserialize) {
            component = it->second.deserialize(obj, data);
        } else {
            core::Logger::Warning(
                "[SceneSerializer] Unknown component type during load: %s (skipping)",
                type.c_str());
        }
    }
    
    // Set component active state
    if (component) {
        component->SetActive(active);
    }
}

json SceneSerializer::SerializeTransformComponent(Component* component) {
    auto* transformComp = dynamic_cast<TransformComponent*>(component);
    if (!transformComp) return json();
    
    json data;
    data["position"] = vec3ToJson(transformComp->GetPosition());
    data["rotation"] = vec3ToJson(transformComp->GetRotation());
    data["scale"] = vec3ToJson(transformComp->GetScale());
    
    return data;
}

Component* SceneSerializer::DeserializeTransformComponent(GameObject* obj, const json& transformJson) {
    auto transform = obj->EnsureTransform();
    
    if (transformJson.contains("position")) {
        transform->SetPosition(jsonToVec3(transformJson["position"]));
    }
    if (transformJson.contains("rotation")) {
        transform->SetRotation(jsonToVec3(transformJson["rotation"]));
    }
    if (transformJson.contains("scale")) {
        transform->SetScale(jsonToVec3(transformJson["scale"]));
    }
    
    return transform.get();
}

json SceneSerializer::SerializeMaterialComponent(Component* component) {
    auto* materialComp = dynamic_cast<MaterialComponent*>(component);
    if (!materialComp) return json();
    
    auto material = materialComp->GetMaterial();
    if (!material) return json();
    
    json data;
    data["name"] = material->GetName();
    data["diffuseColor"] = vec3ToJson(material->GetDiffuseColor());
    data["specularColor"] = vec3ToJson(material->GetSpecularColor());
    data["shininess"] = material->GetShininess();
    data["emissionColor"] = vec3ToJson(material->GetEmissionColor());
    
    // Note: Textures are not serialized (they're loaded from paths)
    // You might want to store texture paths here
    
    return data;
}

Component* SceneSerializer::DeserializeMaterialComponent(GameObject* obj, const json& materialJson) {
    auto materialComp = obj->AddComponent<MaterialComponent>();
    if (!materialComp) return nullptr;
    
    auto material = std::make_shared<Material>();
    
    if (materialJson.contains("name") && materialJson["name"].is_string()) {
        material->SetName(materialJson["name"].get<std::string>());
    }
    if (materialJson.contains("diffuseColor")) {
        material->SetDiffuseColor(jsonToVec3(materialJson["diffuseColor"]));
    }
    if (materialJson.contains("specularColor")) {
        material->SetSpecularColor(jsonToVec3(materialJson["specularColor"]));
    }
    if (materialJson.contains("shininess") && materialJson["shininess"].is_number()) {
        material->SetShininess(materialJson["shininess"].get<float>());
    }
    if (materialJson.contains("emissionColor")) {
        material->SetEmissionColor(jsonToVec3(materialJson["emissionColor"]));
    }
    
    materialComp->SetMaterial(material);
    return materialComp.get();
}

json SceneSerializer::SerializeLightComponent(Component* component) {
    auto* lightComp = dynamic_cast<LightComponent*>(component);
    if (!lightComp) return json();
    
    json data;
    
    // Light type
    int typeInt = static_cast<int>(lightComp->GetType());
    data["type"] = typeInt; // 0=Directional, 1=Point, 2=Spot
    
    data["color"] = vec3ToJson(lightComp->GetColor());
    data["intensity"] = lightComp->GetIntensity();
    data["enabled"] = lightComp->IsEnabled();
    
    if (lightComp->GetType() == LightComponent::LightType::Directional || 
        lightComp->GetType() == LightComponent::LightType::Spot) {
        data["direction"] = vec3ToJson(lightComp->GetDirection());
    }
    
    if (lightComp->GetType() == LightComponent::LightType::Point || 
        lightComp->GetType() == LightComponent::LightType::Spot) {
        glm::vec3 attenuation = lightComp->GetAttenuation();
        data["attenuation"] = json::object({
            {"constant", attenuation.x},
            {"linear", attenuation.y},
            {"quadratic", attenuation.z}
        });
    }
    
    if (lightComp->GetType() == LightComponent::LightType::Spot) {
        data["innerConeAngle"] = glm::degrees(lightComp->GetInnerConeAngle());
        data["outerConeAngle"] = glm::degrees(lightComp->GetOuterConeAngle());
    }
    
    return data;
}

Component* SceneSerializer::DeserializeLightComponent(GameObject* obj, const json& lightJson) {
    auto lightComp = obj->AddComponent<LightComponent>();
    if (!lightComp) return nullptr;
    
    if (lightJson.contains("type") && lightJson["type"].is_number()) {
        int typeInt = lightJson["type"].get<int>();
        lightComp->SetType(static_cast<LightComponent::LightType>(typeInt));
    }
    
    if (lightJson.contains("color")) {
        lightComp->SetColor(jsonToVec3(lightJson["color"]));
    }
    if (lightJson.contains("intensity") && lightJson["intensity"].is_number()) {
        lightComp->SetIntensity(lightJson["intensity"].get<float>());
    }
    if (lightJson.contains("enabled") && lightJson["enabled"].is_boolean()) {
        lightComp->SetEnabled(lightJson["enabled"].get<bool>());
    }
    
    if (lightJson.contains("direction")) {
        lightComp->SetDirection(jsonToVec3(lightJson["direction"]));
    }
    
    if (lightJson.contains("attenuation") && lightJson["attenuation"].is_object()) {
        float constant = lightJson["attenuation"].value("constant", 1.0f);
        float linear = lightJson["attenuation"].value("linear", 0.09f);
        float quadratic = lightJson["attenuation"].value("quadratic", 0.032f);
        lightComp->SetAttenuation(constant, linear, quadratic);
    }
    
    if (lightJson.contains("innerConeAngle") && lightJson.contains("outerConeAngle")) {
        float inner = lightJson["innerConeAngle"].get<float>();
        float outer = lightJson["outerConeAngle"].get<float>();
        lightComp->SetInnerConeAngle(inner);
        lightComp->SetOuterConeAngle(outer);
    }
    
    return lightComp.get();
}

} // namespace gm


