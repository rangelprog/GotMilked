#pragma once
#include <memory>
#include <string>
#include <nlohmann/json_fwd.hpp>

namespace gm {
class Scene;
class GameObject;
class Component;

/**
 * @brief Utility class for serializing and deserializing Scenes to/from JSON
 */
class SceneSerializer {
public:
    // Save scene to JSON file
    static bool SaveToFile(Scene& scene, const std::string& filepath);
    
    // Load scene from JSON file
    static bool LoadFromFile(Scene& scene, const std::string& filepath);
    
    // Serialize scene to JSON string
    static std::string Serialize(Scene& scene);
    
    // Deserialize scene from JSON string
    static bool Deserialize(Scene& scene, const std::string& jsonString);

private:
    // Helper functions for JSON conversion
    static nlohmann::json SerializeGameObject(std::shared_ptr<GameObject> obj);
    static std::shared_ptr<GameObject> DeserializeGameObject(Scene& scene, const nlohmann::json& objectJson);
    
    static nlohmann::json SerializeComponent(Component* component);
    static void DeserializeComponent(GameObject* obj, const nlohmann::json& componentJson);
    
    // Component-specific serialization
    static nlohmann::json SerializeTransformComponent(Component* component);
    static nlohmann::json SerializeMaterialComponent(Component* component);
    static nlohmann::json SerializeLightComponent(Component* component);
    
    static void DeserializeTransformComponent(GameObject* obj, const nlohmann::json& transformJson);
    static void DeserializeMaterialComponent(GameObject* obj, const nlohmann::json& materialJson);
    static void DeserializeLightComponent(GameObject* obj, const nlohmann::json& lightJson);
};

} // namespace gm


