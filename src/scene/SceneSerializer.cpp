#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Component.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/MaterialComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/rendering/Material.hpp"
#include <fstream>
#include <sstream>
#include <cstdio>

// Include JSON library
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace gm {

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
            printf("[SceneSerializer] Error: Failed to open file for writing: %s\n", filepath.c_str());
            return false;
        }
        
        file << jsonString;
        file.close();
        
        printf("[SceneSerializer] Successfully saved scene to: %s\n", filepath.c_str());
        return true;
    } catch (const std::exception& e) {
        printf("[SceneSerializer] Error saving scene: %s\n", e.what());
        return false;
    }
}

bool SceneSerializer::LoadFromFile(Scene& scene, const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            printf("[SceneSerializer] Error: Failed to open file for reading: %s\n", filepath.c_str());
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        return Deserialize(scene, buffer.str());
    } catch (const std::exception& e) {
        printf("[SceneSerializer] Error loading scene: %s\n", e.what());
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
        
        printf("[SceneSerializer] Successfully loaded scene with %zu objects\n", 
               scene.GetAllGameObjects().size());
        return true;
    } catch (const std::exception& e) {
        printf("[SceneSerializer] Error deserializing scene: %s\n", e.what());
        return false;
    }
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
        printf("[SceneSerializer] Warning: GameObject missing name, skipping\n");
        return nullptr;
    }
    
    std::string name = objectJson["name"].get<std::string>();
    auto obj = scene.CreateGameObject(name);
    
    if (!obj) {
        printf("[SceneSerializer] Warning: Failed to create GameObject: %s\n", name.c_str());
        return nullptr;
    }
    
    // Set active state
    if (objectJson.contains("active") && objectJson["active"].is_boolean()) {
        obj->SetActive(objectJson["active"].get<bool>());
    }
    
    // Deserialize tags
    if (objectJson.contains("tags") && objectJson["tags"].is_array()) {
        for (const auto& tagJson : objectJson["tags"]) {
            if (tagJson.is_string()) {
                obj->AddTag(tagJson.get<std::string>());
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
    if (type == "TransformComponent") {
        json data = SerializeTransformComponent(component);
        compJson["data"] = data;
    } else if (type == "MaterialComponent") {
        json data = SerializeMaterialComponent(component);
        compJson["data"] = data;
    } else if (type == "LightComponent") {
        json data = SerializeLightComponent(component);
        compJson["data"] = data;
    } else {
        // Unknown component type - skip or store as empty
        printf("[SceneSerializer] Warning: Unknown component type: %s\n", type.c_str());
        return json(); // Return null to skip
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
    if (type == "TransformComponent") {
        DeserializeTransformComponent(obj, data);
    } else if (type == "MaterialComponent") {
        DeserializeMaterialComponent(obj, data);
    } else if (type == "LightComponent") {
        DeserializeLightComponent(obj, data);
    } else {
        printf("[SceneSerializer] Warning: Unknown component type during deserialization: %s\n", type.c_str());
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

void SceneSerializer::DeserializeTransformComponent(GameObject* obj, const json& transformJson) {
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

void SceneSerializer::DeserializeMaterialComponent(GameObject* obj, const json& materialJson) {
    auto materialComp = obj->AddComponent<MaterialComponent>();
    if (!materialComp) return;
    
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

void SceneSerializer::DeserializeLightComponent(GameObject* obj, const json& lightJson) {
    auto lightComp = obj->AddComponent<LightComponent>();
    if (!lightComp) return;
    
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
}

} // namespace gm


