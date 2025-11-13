#include "SceneSerializerExtensions.hpp"
#include "GameConstants.hpp"
#include "gm/scene/ComponentFactory.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/physics/RigidBodyComponent.hpp"
#if GM_DEBUG_TOOLS
#include "EditableTerrainComponent.hpp"
#endif
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/core/Logger.hpp"
#include "gm/gameplay/CameraRigComponent.hpp"
#include "gm/gameplay/QuestTriggerComponent.hpp"
#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>
#include <algorithm>
#include <vector>

namespace gm {
namespace SceneSerializerExtensions {

void RegisterSerializers() {
    // Register components with the factory
    auto& factory = gm::scene::ComponentFactory::Instance();

    if (!factory.Register<gm::gameplay::CameraRigComponent>("CameraRigComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] CameraRigComponent already registered in factory");
    }
    if (!factory.Register<gm::gameplay::QuestTriggerComponent>("QuestTriggerComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] QuestTriggerComponent already registered in factory");
    }

    #if GM_DEBUG_TOOLS
    using gm::debug::EditableTerrainComponent;
    if (!factory.Register<EditableTerrainComponent>("EditableTerrainComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] EditableTerrainComponent already registered in factory");
    }
    #endif
    if (!factory.Register<gm::scene::StaticMeshComponent>("StaticMeshComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] StaticMeshComponent already registered in factory");
    }
    if (!factory.Register<gm::physics::RigidBodyComponent>("RigidBodyComponent")) {
        gm::core::Logger::Warning("[SceneSerializerExtensions] RigidBodyComponent already registered in factory");
    }
    
    gm::SceneSerializer::RegisterComponentSerializer(
        "CameraRigComponent",
        [](gm::Component* component) -> nlohmann::json {
            auto* rig = dynamic_cast<gm::gameplay::CameraRigComponent*>(component);
            if (!rig) {
                return nlohmann::json();
            }

            const auto& config = rig->GetConfig();
            nlohmann::json data;
            data["rigId"] = rig->GetRigId();
            data["baseSpeed"] = config.baseSpeed;
            data["sprintMultiplier"] = config.sprintMultiplier;
            data["fovMin"] = config.fovMin;
            data["fovMax"] = config.fovMax;
            data["fovScrollSensitivity"] = config.fovScrollSensitivity;
            data["initialFov"] = config.initialFov;
            data["captureMouseOnFocus"] = rig->CaptureMouseOnFocus();
            data["autoActivate"] = rig->AutoActivate();
            return data;
        },
        [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
            if (!data.is_object()) {
                return nullptr;
            }

            auto& factory = gm::scene::ComponentFactory::Instance();
            auto rig = std::dynamic_pointer_cast<gm::gameplay::CameraRigComponent>(
                factory.Create("CameraRigComponent", obj));
            if (!rig) {
                gm::core::Logger::Error("[SceneSerializer] Failed to create CameraRigComponent");
                return nullptr;
            }

            auto config = rig->GetConfig();
            if (data.contains("baseSpeed") && data["baseSpeed"].is_number()) {
                config.baseSpeed = data["baseSpeed"].get<float>();
            }
            if (data.contains("sprintMultiplier") && data["sprintMultiplier"].is_number()) {
                config.sprintMultiplier = data["sprintMultiplier"].get<float>();
            }
            if (data.contains("fovMin") && data["fovMin"].is_number()) {
                config.fovMin = data["fovMin"].get<float>();
            }
            if (data.contains("fovMax") && data["fovMax"].is_number()) {
                config.fovMax = data["fovMax"].get<float>();
            }
            if (data.contains("fovScrollSensitivity") && data["fovScrollSensitivity"].is_number()) {
                config.fovScrollSensitivity = data["fovScrollSensitivity"].get<float>();
            }
            if (data.contains("initialFov") && data["initialFov"].is_number()) {
                config.initialFov = data["initialFov"].get<float>();
            }
            rig->SetConfig(config);

            if (data.contains("rigId") && data["rigId"].is_string()) {
                rig->SetRigId(data["rigId"].get<std::string>());
            }
            if (data.contains("captureMouseOnFocus") && data["captureMouseOnFocus"].is_boolean()) {
                rig->SetCaptureMouseOnFocus(data["captureMouseOnFocus"].get<bool>());
            }
            if (data.contains("autoActivate") && data["autoActivate"].is_boolean()) {
                rig->SetAutoActivate(data["autoActivate"].get<bool>());
            }

            return rig.get();
        });

    gm::SceneSerializer::RegisterComponentSerializer(
        "QuestTriggerComponent",
        [](gm::Component* component) -> nlohmann::json {
            auto* quest = dynamic_cast<gm::gameplay::QuestTriggerComponent*>(component);
            if (!quest) {
                return nlohmann::json();
            }
            nlohmann::json data;
            data["questId"] = quest->GetQuestId();
            data["activationRadius"] = quest->GetActivationRadius();
            data["triggerOnSceneLoad"] = quest->TriggerOnSceneLoad();
            data["triggerOnInteract"] = quest->TriggerOnInteract();
            data["repeatable"] = quest->IsRepeatable();
            data["activationAction"] = quest->GetActivationAction();
            return data;
        },
        [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
            if (!data.is_object()) {
                return nullptr;
            }

            auto& factory = gm::scene::ComponentFactory::Instance();
            auto quest = std::dynamic_pointer_cast<gm::gameplay::QuestTriggerComponent>(
                factory.Create("QuestTriggerComponent", obj));
            if (!quest) {
                gm::core::Logger::Error("[SceneSerializer] Failed to create QuestTriggerComponent");
                return nullptr;
            }

            if (data.contains("questId") && data["questId"].is_string()) {
                quest->SetQuestId(data["questId"].get<std::string>());
            }
            if (data.contains("activationRadius") && data["activationRadius"].is_number()) {
                quest->SetActivationRadius(data["activationRadius"].get<float>());
            }
            if (data.contains("triggerOnSceneLoad") && data["triggerOnSceneLoad"].is_boolean()) {
                quest->SetTriggerOnSceneLoad(data["triggerOnSceneLoad"].get<bool>());
            }
            if (data.contains("triggerOnInteract") && data["triggerOnInteract"].is_boolean()) {
                quest->SetTriggerOnInteract(data["triggerOnInteract"].get<bool>());
            }
            if (data.contains("repeatable") && data["repeatable"].is_boolean()) {
                quest->SetRepeatable(data["repeatable"].get<bool>());
            }
            if (data.contains("activationAction") && data["activationAction"].is_string()) {
                quest->SetActivationAction(data["activationAction"].get<std::string>());
            }

            return quest.get();
        });

    #if GM_DEBUG_TOOLS
    // Register EditableTerrainComponent serializer
    gm::SceneSerializer::RegisterComponentSerializer(
        "EditableTerrainComponent",
        [](gm::Component* component) -> nlohmann::json {
            auto* terrain = dynamic_cast<EditableTerrainComponent*>(component);
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
            data["textureTiling"] = terrain->GetTextureTiling();
            data["baseTextureGuid"] = terrain->GetBaseTextureGuid();
            data["activePaintLayer"] = terrain->GetActivePaintLayerIndex();
            
            // Serialize height array
            const auto& heights = terrain->GetHeights();
            data["heights"] = nlohmann::json::array();
            for (float height : heights) {
                data["heights"].push_back(height);
            }

            nlohmann::json paintLayers = nlohmann::json::array();
            const int paintLayerCount = terrain->GetPaintLayerCount();
            for (int i = 0; i < paintLayerCount; ++i) {
                nlohmann::json layerJson;
                layerJson["guid"] = terrain->GetPaintTextureGuid(i);
                layerJson["enabled"] = terrain->IsPaintLayerEnabled(i);
                const auto& weights = terrain->GetPaintLayerWeights(i);
                layerJson["weights"] = weights;
                paintLayers.push_back(std::move(layerJson));
            }
            data["paintLayers"] = paintLayers;
            
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
            gm::core::Logger::Info("[SceneSerializer] Created EditableTerrainComponent for GameObject '{}'", obj->GetName());
            
            // Deserialize terrain data
            int resolution = data.value("resolution", gotmilked::GameConstants::Terrain::DefaultResolution);
            float size = data.value("size", gotmilked::GameConstants::Terrain::DefaultSize);
            float minHeight = data.value("minHeight", gotmilked::GameConstants::Terrain::DefaultMinHeight);
            float maxHeight = data.value("maxHeight", gotmilked::GameConstants::Terrain::DefaultMaxHeight);
            
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

            terrain->SetTextureTiling(data.value("textureTiling", terrain->GetTextureTiling()));

            if (data.contains("baseTextureGuid") && data["baseTextureGuid"].is_string()) {
                terrain->SetBaseTextureGuidFromSave(data["baseTextureGuid"].get<std::string>());
            }

            int activePaintLayer = data.value("activePaintLayer", 0);
            if (data.contains("paintLayers") && data["paintLayers"].is_array()) {
                const auto& layers = data["paintLayers"];
                terrain->SetPaintLayerCount(std::max(1, static_cast<int>(layers.size())));
                std::vector<float> weights;
                for (std::size_t i = 0; i < layers.size() && i < gm::debug::EditableTerrainComponent::kMaxPaintLayers; ++i) {
                    const auto& layerJson = layers[i];
                    std::string guid = layerJson.value("guid", std::string());
                    bool enabled = layerJson.value("enabled", true);
                    weights.clear();
                    if (layerJson.contains("weights") && layerJson["weights"].is_array()) {
                        weights.reserve(layerJson["weights"].size());
                        for (const auto& value : layerJson["weights"]) {
                            if (value.is_number()) {
                                weights.push_back(value.get<float>());
                            }
                        }
                    }
                    terrain->SetPaintLayerData(static_cast<int>(i), guid, enabled, weights);
                }
                terrain->SetActivePaintLayerIndex(activePaintLayer);
            }

            terrain->MarkMeshDirty();
            
            // Verify component was added to GameObject
            auto verify = obj->GetComponent<EditableTerrainComponent>();
            if (!verify) {
                gm::core::Logger::Error("[SceneSerializer] EditableTerrainComponent was not found on GameObject after creation!");
            } else {
                gm::core::Logger::Info("[SceneSerializer] Verified EditableTerrainComponent is on GameObject");
            }
            
            return terrain.get();
        }
    );
    #endif

    // Register StaticMeshComponent serializer
    gm::SceneSerializer::RegisterComponentSerializer(
        "StaticMeshComponent",
        [](gm::Component* component) -> nlohmann::json {
            auto* meshComp = dynamic_cast<gm::scene::StaticMeshComponent*>(component);
            if (!meshComp) {
                gm::core::Logger::Warning("[SceneSerializer] StaticMeshComponent serialization: component is null");
                return nlohmann::json();
            }
            
            nlohmann::json data;
            data["hasComponent"] = true;
            data["version"] = 1; // Version for future compatibility
            
            // Serialize resource GUIDs
            const std::string& meshGuid = meshComp->GetMeshGuid();
            const std::string& shaderGuid = meshComp->GetShaderGuid();
            const std::string& materialGuid = meshComp->GetMaterialGuid();
            
            // Validate that we have at least mesh and shader (required for rendering)
            bool hasRequiredResources = meshComp->GetMesh() != nullptr && meshComp->GetShader() != nullptr;
            bool hasRequiredGUIDs = !meshGuid.empty() && !shaderGuid.empty();
            
            if (!hasRequiredGUIDs && hasRequiredResources) {
                gm::core::Logger::Warning(
                    "[SceneSerializer] StaticMeshComponent on GameObject '{}' has resources but no GUIDs. "
                    "Resources will not be restored after load. Mesh GUID: {}, Shader GUID: {}",
                    meshComp->GetOwner() ? meshComp->GetOwner()->GetName() : "unknown",
                    meshGuid.empty() ? "missing" : meshGuid,
                    shaderGuid.empty() ? "missing" : shaderGuid);
            }
            
            if (!meshGuid.empty()) {
                data["meshGuid"] = meshGuid;
            } else if (hasRequiredResources) {
                gm::core::Logger::Warning("[SceneSerializer] StaticMeshComponent missing mesh GUID");
            }
            
            if (!shaderGuid.empty()) {
                data["shaderGuid"] = shaderGuid;
            } else if (hasRequiredResources) {
                gm::core::Logger::Warning("[SceneSerializer] StaticMeshComponent missing shader GUID");
            }
            
            if (!materialGuid.empty()) {
                data["materialGuid"] = materialGuid;
            }
            
            // Log serialization success
            gm::core::Logger::Debug(
                "[SceneSerializer] Serialized StaticMeshComponent: meshGuid={}, shaderGuid={}, materialGuid={}",
                meshGuid.empty() ? "(none)" : meshGuid,
                shaderGuid.empty() ? "(none)" : shaderGuid,
                materialGuid.empty() ? "(none)" : materialGuid);
            
            return data;
        },
        [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
            if (!data.is_object()) {
                gm::core::Logger::Error("[SceneSerializer] StaticMeshComponent deserialization: data is not an object");
                return nullptr;
            }
            
            if (!obj) {
                gm::core::Logger::Error("[SceneSerializer] StaticMeshComponent deserialization: GameObject is null");
                return nullptr;
            }
            
            // Check version for future compatibility
            int version = data.value("version", 1);
            if (version > 1) {
                gm::core::Logger::Warning(
                    "[SceneSerializer] StaticMeshComponent version %d is newer than supported (1). "
                    "Some features may not be restored correctly.",
                    version);
            }
            
            auto& factory = gm::scene::ComponentFactory::Instance();
            auto meshComp = std::dynamic_pointer_cast<gm::scene::StaticMeshComponent>(
                factory.Create("StaticMeshComponent", obj));
            if (!meshComp) {
                gm::core::Logger::Error(
                    "[SceneSerializer] Failed to create StaticMeshComponent for GameObject '%s'",
                    obj->GetName().c_str());
                return nullptr;
            }
            
            // Restore GUIDs from serialized data
            // Resource pointers will be restored later via RestoreResources()
            bool hasMeshGuid = false;
            bool hasShaderGuid = false;
            bool hasMaterialGuid = false;
            
            if (data.contains("meshGuid")) {
                if (data["meshGuid"].is_string()) {
                    std::string meshGuid = data["meshGuid"].get<std::string>();
                    if (!meshGuid.empty()) {
                        meshComp->SetMesh(nullptr, meshGuid);
                        hasMeshGuid = true;
                    } else {
                        gm::core::Logger::Warning(
                            "[SceneSerializer] StaticMeshComponent on GameObject '%s' has empty mesh GUID",
                            obj->GetName().c_str());
                    }
                } else {
                    gm::core::Logger::Warning(
                        "[SceneSerializer] StaticMeshComponent on GameObject '%s' has invalid meshGuid type (expected string)",
                        obj->GetName().c_str());
                }
            }
            
            if (data.contains("shaderGuid")) {
                if (data["shaderGuid"].is_string()) {
                    std::string shaderGuid = data["shaderGuid"].get<std::string>();
                    if (!shaderGuid.empty()) {
                        meshComp->SetShader(nullptr, shaderGuid);
                        hasShaderGuid = true;
                    } else {
                        gm::core::Logger::Warning(
                            "[SceneSerializer] StaticMeshComponent on GameObject '%s' has empty shader GUID",
                            obj->GetName().c_str());
                    }
                } else {
                    gm::core::Logger::Warning(
                        "[SceneSerializer] StaticMeshComponent on GameObject '%s' has invalid shaderGuid type (expected string)",
                        obj->GetName().c_str());
                }
            }
            
            if (data.contains("materialGuid")) {
                if (data["materialGuid"].is_string()) {
                    std::string materialGuid = data["materialGuid"].get<std::string>();
                    if (!materialGuid.empty()) {
                        meshComp->SetMaterial(nullptr, materialGuid);
                        hasMaterialGuid = true;
                    }
                } else {
                    gm::core::Logger::Warning(
                        "[SceneSerializer] StaticMeshComponent on GameObject '%s' has invalid materialGuid type (expected string)",
                        obj->GetName().c_str());
                }
            }
            
            // Validate required GUIDs
            if (!hasMeshGuid || !hasShaderGuid) {
                gm::core::Logger::Warning(
                    "[SceneSerializer] StaticMeshComponent on GameObject '%s' is missing required GUIDs "
                    "(mesh: %s, shader: %s). Component may not render correctly after resource restoration.",
                    obj->GetName().c_str(),
                    hasMeshGuid ? "present" : "missing",
                    hasShaderGuid ? "present" : "missing");
            }
            
            gm::core::Logger::Debug(
                "[SceneSerializer] Deserialized StaticMeshComponent for GameObject '%s': "
                "meshGuid=%s, shaderGuid=%s, materialGuid=%s",
                obj->GetName().c_str(),
                meshComp->GetMeshGuid().empty() ? "(none)" : meshComp->GetMeshGuid().c_str(),
                meshComp->GetShaderGuid().empty() ? "(none)" : meshComp->GetShaderGuid().c_str(),
                meshComp->GetMaterialGuid().empty() ? "(none)" : meshComp->GetMaterialGuid().c_str());
            
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
            
            gm::core::Logger::Info("[SceneSerializer] Created RigidBodyComponent for GameObject '{}'",
                obj->GetName());
            
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
#if GM_DEBUG_TOOLS
    gm::SceneSerializer::UnregisterComponentSerializer("EditableTerrainComponent");
#endif
    gm::SceneSerializer::UnregisterComponentSerializer("CameraRigComponent");
    gm::SceneSerializer::UnregisterComponentSerializer("QuestTriggerComponent");
    gm::SceneSerializer::UnregisterComponentSerializer("StaticMeshComponent");
    gm::SceneSerializer::UnregisterComponentSerializer("RigidBodyComponent");

    auto& factory = gm::scene::ComponentFactory::Instance();
    factory.Unregister("CameraRigComponent");
    factory.Unregister("QuestTriggerComponent");
    #if GM_DEBUG_TOOLS
    factory.Unregister("EditableTerrainComponent");
    #endif
    factory.Unregister("StaticMeshComponent");
    factory.Unregister("RigidBodyComponent");
}

} // namespace SceneSerializerExtensions
} // namespace gm

