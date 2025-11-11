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
    // nlohmann::json doesn't support reserve(), but we can use initializer list for efficiency
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
    // nlohmann::json doesn't support reserve(), but we can pre-allocate with initializer list
    // or just build the array (nlohmann::json handles memory efficiently internally)
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
        // Build JSON object
        json sceneJson;
        
        // Scene metadata
        sceneJson["name"] = scene.GetName();
        sceneJson["isPaused"] = scene.IsPaused();
        
        // Serialize all GameObjects
        const auto& allObjects = scene.GetAllGameObjects();
        json gameObjectsJson = json::array();
        for (const auto& obj : allObjects) {
            if (obj && !obj->IsDestroyed()) {
                gameObjectsJson.push_back(SerializeGameObject(obj));
            }
        }
        sceneJson["gameObjects"] = gameObjectsJson;
        
        // Write directly to file with compact format (no indentation) to reduce memory
        // Use compact format for smaller file size and less memory during dump
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            core::Logger::Error("[SceneSerializer] Failed to open file for writing: %s",
                                filepath.c_str());
            return false;
        }
        
        // Use compact format (indent = -1) to reduce memory usage
        // For debugging, use indent = 4, for production use indent = -1
        file << sceneJson.dump(-1);  // -1 = compact (no indentation, minimal whitespace)
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
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            core::Logger::Error("[SceneSerializer] Failed to open file for reading: %s",
                                filepath.c_str());
            return false;
        }
        
        // Get file size to reserve buffer capacity and avoid reallocations
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Reserve capacity to avoid reallocations during file read
        std::string buffer;
        if (fileSize > 0) {
            buffer.reserve(static_cast<size_t>(fileSize));
        }
        
        buffer.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        file.close();
        
        return Deserialize(scene, buffer);
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
    const auto& allObjects = scene.GetAllGameObjects();
    json gameObjectsJson = json::array();
    // Note: nlohmann::json doesn't support reserve(), but handles memory efficiently internally
    for (const auto& obj : allObjects) {
        if (obj && !obj->IsDestroyed()) {
            gameObjectsJson.push_back(SerializeGameObject(obj));
        }
    }
    sceneJson["gameObjects"] = gameObjectsJson;
    
    // Note: Camera data is not stored in Scene, it's managed by Game
    // Camera position/rotation should be saved separately or via callbacks
    
    // Use compact format (indent = -1) to reduce memory usage
    // For debugging/readability, use indent = 4, for production use indent = -1
    return sceneJson.dump(-1);  // -1 = compact (no indentation, minimal whitespace)
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
    const auto& tags = obj->GetTags();
    json tagsJson = json::array();
    // Note: nlohmann::json doesn't support reserve(), but handles memory efficiently internally
    for (const auto& tag : tags) {
        tagsJson.push_back(tag);
    }
    objJson["tags"] = tagsJson;
    
    // Serialize components (save all components, including inactive ones)
    const auto& components = obj->GetComponents();
    json componentsJson = json::array();
    // Note: nlohmann::json doesn't support reserve(), but handles memory efficiently internally
    for (const auto& component : components) {
        if (component) {
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
        // Note: Scene will need to mark active lists as dirty after deserialization
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
    // GetName() returns const reference, use it directly to avoid copy
    const std::string& type = component->GetName();
    compJson["type"] = type;
    compJson["active"] = component->IsActive();
    
    core::Logger::Debug("[SceneSerializer] Serializing component type: %s", type.c_str());
    
    // Component-specific serialization
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
        core::Logger::Debug("[SceneSerializer] Looking for custom serializer for type: %s (registry size: %zu)", 
            type.c_str(), registry.size());
        auto it = registry.find(type);
        if (it != registry.end() && it->second.serialize) {
            core::Logger::Info("[SceneSerializer] Found custom serializer for type: %s", type.c_str());
            json customData = it->second.serialize(component);
            if (!customData.is_null() && !customData.empty()) {
                compJson["data"] = customData;
                core::Logger::Info("[SceneSerializer] Successfully serialized component: %s", type.c_str());
            } else {
                core::Logger::Warning("[SceneSerializer] Custom serializer returned empty data for type: %s", type.c_str());
                return json(); // Return null to skip
            }
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
        core::Logger::Warning("[SceneSerializer] Component missing type field");
        return;
    }
    
    std::string type = componentJson["type"].get<std::string>();
    core::Logger::Debug("[SceneSerializer] Deserializing component type: %s for GameObject: %s", 
        type.c_str(), obj ? obj->GetName().c_str() : "null");
    
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
        core::Logger::Debug("[SceneSerializer] Looking for custom serializer for type: %s (registry size: %zu)", 
            type.c_str(), registry.size());
        auto it = registry.find(type);
        if (it != registry.end() && it->second.deserialize) {
            core::Logger::Info("[SceneSerializer] Found custom deserializer for type: %s", type.c_str());
            component = it->second.deserialize(obj, data);
            if (component) {
                core::Logger::Info("[SceneSerializer] Custom deserializer returned component: %s", 
                    component->GetName().c_str());
            } else {
                core::Logger::Warning("[SceneSerializer] Custom deserializer returned null for type: %s", type.c_str());
            }
        } else {
            core::Logger::Warning(
                "[SceneSerializer] Unknown component type during load: %s (skipping)",
                type.c_str());
        }
    }
    
    // Set component active state
    if (component) {
        component->SetActive(active);
        core::Logger::Debug("[SceneSerializer] Component %s set active=%s", 
            component->GetName().c_str(), active ? "true" : "false");
    } else {
        core::Logger::Warning("[SceneSerializer] Failed to deserialize component type: %s", type.c_str());
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


