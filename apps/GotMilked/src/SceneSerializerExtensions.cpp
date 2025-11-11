#include "SceneSerializerExtensions.hpp"
#include "gm/scene/ComponentFactory.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/physics/RigidBodyComponent.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/core/Logger.hpp"
#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>

namespace gm {
namespace SceneSerializerExtensions {

void RegisterSerializers() {
    // Register components with the factory
    auto& factory = gm::scene::ComponentFactory::Instance();
    
    if (!factory.Register<EditableTerrainComponent>("EditableTerrainComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] EditableTerrainComponent already registered in factory");
    }
    if (!factory.Register<gm::scene::StaticMeshComponent>("StaticMeshComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] StaticMeshComponent already registered in factory");
    }
    if (!factory.Register<gm::physics::RigidBodyComponent>("RigidBodyComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] RigidBodyComponent already registered in factory");
    }
    
    // Register EditableTerrainComponent serializer
    gm::SceneSerializer::RegisterComponentSerializer(
        "EditableTerrainComponent",
        [](gm::Component* component) -> nlohmann::json {
            auto* terrain = dynamic_cast<::EditableTerrainComponent*>(component);
            if (!terrain) {
                return nlohmann::json();
            }
            
            nlohmann::json data;
            data["resolution"] = terrain->GetResolution();
            data["size"] = terrain->GetTerrainSize();
            data["minHeight"] = terrain->GetMinHeight();
            data["maxHeight"] = terrain->GetMaxHeight();
            data["editorWindowVisible"] = terrain->IsEditorWindowVisible();
            data["editingEnabled"] = terrain->IsEditingEnabled();
            
            // Serialize height array
            const auto& heights = terrain->GetHeights();
            data["heights"] = nlohmann::json::array();
            for (float height : heights) {
                data["heights"].push_back(height);
            }
            
            return data;
        },
        [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
            if (!data.is_object()) {
                return nullptr;
            }
            
            auto& factory = gm::scene::ComponentFactory::Instance();
            auto terrain = std::dynamic_pointer_cast<EditableTerrainComponent>(
                factory.Create("EditableTerrainComponent", obj));
            if (!terrain) {
                gm::core::Logger::Error("[SceneSerializer] Failed to create EditableTerrainComponent");
                return nullptr;
            }
            gm::core::Logger::Info("[SceneSerializer] Created EditableTerrainComponent for GameObject '%s'", obj->GetName().c_str());
            
            // Deserialize terrain data
            int resolution = data.value("resolution", 33);
            float size = data.value("size", 20.0f);
            float minHeight = data.value("minHeight", -2.0f);
            float maxHeight = data.value("maxHeight", 4.0f);
            
            std::vector<float> heights;
            if (data.contains("heights") && data["heights"].is_array()) {
                heights.reserve(data["heights"].size());
                for (const auto& heightVal : data["heights"]) {
                    if (heightVal.is_number()) {
                        heights.push_back(heightVal.get<float>());
                    }
                }
            }
            
            // Set height data if we have valid data
            if (!heights.empty() && heights.size() == static_cast<size_t>(resolution * resolution)) {
                if (!terrain->SetHeightData(resolution, size, minHeight, maxHeight, heights)) {
                    gm::core::Logger::Warning("[SceneSerializer] Failed to set terrain height data");
                }
            } else {
                // If no height data or invalid size, use default initialization
                terrain->SetResolution(resolution);
                terrain->SetTerrainSize(size);
            }
            
            // Restore editor window visibility state
            if (data.contains("editorWindowVisible") && data["editorWindowVisible"].is_boolean()) {
                terrain->SetEditorWindowVisible(data["editorWindowVisible"].get<bool>());
            }
            if (data.contains("editingEnabled") && data["editingEnabled"].is_boolean()) {
                // Note: There's no public SetEditingEnabled, so we'll skip this for now
                // The user can re-enable editing through the UI
            }
            
            // Verify component was added to GameObject
            auto verify = obj->GetComponent<::EditableTerrainComponent>();
            if (!verify) {
                gm::core::Logger::Error("[SceneSerializer] EditableTerrainComponent was not found on GameObject after creation!");
            } else {
                gm::core::Logger::Info("[SceneSerializer] Verified EditableTerrainComponent is on GameObject");
            }
            
            return terrain.get();
        }
    );

    // Register StaticMeshComponent serializer
    gm::SceneSerializer::RegisterComponentSerializer(
        "StaticMeshComponent",
        [](gm::Component* component) -> nlohmann::json {
            auto* meshComp = dynamic_cast<gm::scene::StaticMeshComponent*>(component);
            if (!meshComp) {
                return nlohmann::json();
            }
            
            // Note: StaticMeshComponent stores raw pointers to resources
            // We can't serialize the actual resource references without GUIDs
            // For now, we just serialize that the component exists
            // TODO: Add resource GUID storage to StaticMeshComponent for full serialization
            nlohmann::json data;
            data["hasComponent"] = true;
            
            return data;
        },
        [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
            if (!data.is_object()) {
                return nullptr;
            }
            
            auto& factory = gm::scene::ComponentFactory::Instance();
            auto meshComp = std::dynamic_pointer_cast<gm::scene::StaticMeshComponent>(
                factory.Create("StaticMeshComponent", obj));
            if (!meshComp) {
                gm::core::Logger::Error("[SceneSerializer] Failed to create StaticMeshComponent");
                return nullptr;
            }
            
            gm::core::Logger::Info("[SceneSerializer] Created StaticMeshComponent for GameObject '%s'", 
                obj->GetName().c_str());
            
            // Note: Resource references (mesh, shader, material) need to be restored
            // by ApplyResourcesToScene() or similar mechanism after deserialization
            // TODO: Restore resource references from GUIDs when GUID storage is added
            
            return meshComp.get();
        }
    );

    // Register RigidBodyComponent serializer
    gm::SceneSerializer::RegisterComponentSerializer(
        "RigidBodyComponent",
        [](gm::Component* component) -> nlohmann::json {
            auto* rigidBody = dynamic_cast<gm::physics::RigidBodyComponent*>(component);
            if (!rigidBody) {
                return nlohmann::json();
            }
            
            nlohmann::json data;
            
            // Serialize body type
            std::string bodyTypeStr = (rigidBody->GetBodyType() == gm::physics::RigidBodyComponent::BodyType::Static) 
                ? "Static" : "Dynamic";
            data["bodyType"] = bodyTypeStr;
            
            // Serialize collider shape
            std::string colliderShapeStr = (rigidBody->GetColliderShape() == gm::physics::RigidBodyComponent::ColliderShape::Plane)
                ? "Plane" : "Box";
            data["colliderShape"] = colliderShapeStr;
            
            // Serialize plane parameters
            data["planeNormal"] = {
                rigidBody->GetPlaneNormal().x,
                rigidBody->GetPlaneNormal().y,
                rigidBody->GetPlaneNormal().z
            };
            data["planeConstant"] = rigidBody->GetPlaneConstant();
            
            // Serialize box parameters
            glm::vec3 boxHalfExtent = rigidBody->GetBoxHalfExtent();
            data["boxHalfExtent"] = {
                boxHalfExtent.x,
                boxHalfExtent.y,
                boxHalfExtent.z
            };
            
            // Serialize mass
            data["mass"] = rigidBody->GetMass();
            
            return data;
        },
        [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
            if (!data.is_object()) {
                return nullptr;
            }
            
            auto& factory = gm::scene::ComponentFactory::Instance();
            auto rigidBody = std::dynamic_pointer_cast<gm::physics::RigidBodyComponent>(
                factory.Create("RigidBodyComponent", obj));
            if (!rigidBody) {
                gm::core::Logger::Error("[SceneSerializer] Failed to create RigidBodyComponent");
                return nullptr;
            }
            
            gm::core::Logger::Info("[SceneSerializer] Created RigidBodyComponent for GameObject '%s'", 
                obj->GetName().c_str());
            
            // Deserialize body type
            if (data.contains("bodyType") && data["bodyType"].is_string()) {
                std::string bodyTypeStr = data["bodyType"].get<std::string>();
                if (bodyTypeStr == "Static") {
                    rigidBody->SetBodyType(gm::physics::RigidBodyComponent::BodyType::Static);
                } else {
                    rigidBody->SetBodyType(gm::physics::RigidBodyComponent::BodyType::Dynamic);
                }
            }
            
            // Deserialize collider shape
            if (data.contains("colliderShape") && data["colliderShape"].is_string()) {
                std::string colliderShapeStr = data["colliderShape"].get<std::string>();
                if (colliderShapeStr == "Plane") {
                    rigidBody->SetColliderShape(gm::physics::RigidBodyComponent::ColliderShape::Plane);
                } else {
                    rigidBody->SetColliderShape(gm::physics::RigidBodyComponent::ColliderShape::Box);
                }
            }
            
            // Deserialize plane parameters
            if (data.contains("planeNormal") && data["planeNormal"].is_array() && data["planeNormal"].size() == 3) {
                glm::vec3 normal(
                    data["planeNormal"][0].get<float>(),
                    data["planeNormal"][1].get<float>(),
                    data["planeNormal"][2].get<float>()
                );
                rigidBody->SetPlaneNormal(normal);
            }
            if (data.contains("planeConstant") && data["planeConstant"].is_number()) {
                rigidBody->SetPlaneConstant(data["planeConstant"].get<float>());
            }
            
            // Deserialize box parameters
            if (data.contains("boxHalfExtent") && data["boxHalfExtent"].is_array() && data["boxHalfExtent"].size() == 3) {
                glm::vec3 halfExtent(
                    data["boxHalfExtent"][0].get<float>(),
                    data["boxHalfExtent"][1].get<float>(),
                    data["boxHalfExtent"][2].get<float>()
                );
                rigidBody->SetBoxHalfExtent(halfExtent);
            }
            
            // Deserialize mass
            if (data.contains("mass") && data["mass"].is_number()) {
                rigidBody->SetMass(data["mass"].get<float>());
            }
            
            // Note: Physics body will be created when component's Init() is called
            // This happens automatically during scene initialization
            
            return rigidBody.get();
        }
    );
}

void UnregisterSerializers() {
    gm::SceneSerializer::UnregisterComponentSerializer("EditableTerrainComponent");
    gm::SceneSerializer::UnregisterComponentSerializer("StaticMeshComponent");
    gm::SceneSerializer::UnregisterComponentSerializer("RigidBodyComponent");
    
    // Optionally unregister from factory (usually not needed, but available)
    auto& factory = gm::scene::ComponentFactory::Instance();
    factory.Unregister("EditableTerrainComponent");
    factory.Unregister("StaticMeshComponent");
    factory.Unregister("RigidBodyComponent");
}

} // namespace SceneSerializerExtensions
} // namespace gm

