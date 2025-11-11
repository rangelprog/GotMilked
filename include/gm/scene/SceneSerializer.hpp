#pragma once
#include <memory>
#include <string>
#include <functional>
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
    using SerializeCallback = std::function<nlohmann::json(Component*)>;
    using DeserializeCallback = std::function<Component*(GameObject*, const nlohmann::json&)>;

    // Save scene to JSON file
    static bool SaveToFile(Scene& scene, const std::string& filepath);
    
    // Load scene from JSON file
    static bool LoadFromFile(Scene& scene, const std::string& filepath);
    
    // Serialize scene to JSON string
    static std::string Serialize(Scene& scene);
    
    // Deserialize scene from JSON string
    static bool Deserialize(Scene& scene, const std::string& jsonString);
    
    // Custom component serialization registration
    static void RegisterComponentSerializer(const std::string& typeName,
                                            SerializeCallback serializer,
                                            DeserializeCallback deserializer);
    static void UnregisterComponentSerializer(const std::string& typeName);
    static void ClearComponentSerializers();

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
    
    static Component* DeserializeTransformComponent(GameObject* obj, const nlohmann::json& transformJson);
    static Component* DeserializeMaterialComponent(GameObject* obj, const nlohmann::json& materialJson);
    static Component* DeserializeLightComponent(GameObject* obj, const nlohmann::json& lightJson);
};

} // namespace gm


