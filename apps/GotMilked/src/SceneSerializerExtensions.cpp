#include "SceneSerializerExtensions.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/core/Logger.hpp"
#include <nlohmann/json.hpp>

namespace gm {
namespace SceneSerializerExtensions {

namespace {
    // Helper function to create terrain component
    std::shared_ptr<EditableTerrainComponent> CreateTerrainComponent(gm::GameObject* obj) {
        return obj->AddComponent<EditableTerrainComponent>();
    }
}

void RegisterSerializers() {
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
            
            auto terrain = CreateTerrainComponent(obj);
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
}

void UnregisterSerializers() {
    gm::SceneSerializer::UnregisterComponentSerializer("EditableTerrainComponent");
}

} // namespace SceneSerializerExtensions
} // namespace gm

