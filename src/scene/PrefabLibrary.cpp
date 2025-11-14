#define GLM_ENABLE_EXPERIMENTAL
#include "gm/scene/PrefabLibrary.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/core/Logger.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace gm::scene {

namespace {

struct PrefabValidationResult {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    bool IsValid() const { return errors.empty(); }
};

bool IsPrefabFile(const std::filesystem::path& path) {
    return path.has_filename() && path.extension() == ".json";
}

glm::mat4 MakeTransform(const glm::vec3& position, const glm::vec3& rotationEulerDegrees, const glm::vec3& scale) {
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, position);
    transform = glm::rotate(transform, glm::radians(rotationEulerDegrees.x), glm::vec3(1, 0, 0));
    transform = glm::rotate(transform, glm::radians(rotationEulerDegrees.y), glm::vec3(0, 1, 0));
    transform = glm::rotate(transform, glm::radians(rotationEulerDegrees.z), glm::vec3(0, 0, 1));
    transform = glm::scale(transform, scale);
    return transform;
}

void ApplyTransformToGameObject(const std::shared_ptr<gm::GameObject>& object, const glm::mat4& rootTransform) {
    if (!object) {
        return;
    }
    auto transform = object->EnsureTransform();
    if (!transform) {
        return;
    }

    glm::mat4 current = transform->GetMatrix();
    glm::mat4 composed = rootTransform * current;

    glm::vec3 translation;
    glm::vec3 scaling;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat orientation;
    if (!glm::decompose(composed, scaling, orientation, translation, skew, perspective)) {
        return;
    }
    (void)skew;
    (void)perspective;

    glm::vec3 rotationEulerDegrees = glm::degrees(glm::eulerAngles(orientation));

    transform->SetPosition(translation);
    transform->SetRotation(rotationEulerDegrees);
    transform->SetScale(scaling);
}

void ValidateComponent(const nlohmann::json& componentJson, std::string_view prefabName, std::size_t objectIndex, std::size_t componentIndex, PrefabValidationResult& result) {
    const std::string context = fmt::format("Prefab '{}' object[{}] component[{}]", prefabName, objectIndex, componentIndex);
    if (!componentJson.is_object()) {
        result.errors.push_back(fmt::format("{}: component entry must be an object", context));
        return;
    }
    if (!componentJson.contains("type") || !componentJson["type"].is_string()) {
        result.errors.push_back(fmt::format("{}: missing required string field 'type'", context));
    }
    if (componentJson.contains("data") && !componentJson["data"].is_object()) {
        result.warnings.push_back(fmt::format("{}: optional 'data' field should be an object", context));
    }
    if (componentJson.contains("active") && !componentJson["active"].is_boolean()) {
        result.warnings.push_back(fmt::format("{}: optional 'active' field should be a boolean", context));
    }

    const std::string type = componentJson.value("type", "");
    if ((type == "StaticMeshComponent" || type == "SkinnedMeshComponent") &&
        componentJson.contains("data") && componentJson["data"].is_object()) {
        const auto& data = componentJson["data"];
        const std::string meshGuid = data.value("meshGuid", "");
        const std::string materialGuid = data.value("materialGuid", "");
        if (!meshGuid.empty() && materialGuid.empty()) {
            result.warnings.push_back(
                fmt::format("{}: {} references mesh '{}' without a material assignment", context, type, meshGuid));
        }
    }
}

void ValidateGameObject(const nlohmann::json& objectJson, std::string_view prefabName, std::size_t objectIndex, PrefabValidationResult& result) {
    const std::string context = fmt::format("Prefab '{}' object[{}]", prefabName, objectIndex);
    if (!objectJson.is_object()) {
        result.errors.push_back(fmt::format("{}: entry must be an object", context));
        return;
    }
    if (!objectJson.contains("name") || !objectJson["name"].is_string()) {
        result.errors.push_back(fmt::format("{}: missing required string field 'name'", context));
    }
    if (objectJson.contains("components")) {
        if (!objectJson["components"].is_array()) {
            result.errors.push_back(fmt::format("{}: 'components' must be an array", context));
        } else {
            std::size_t componentIndex = 0;
            for (const auto& componentJson : objectJson["components"]) {
                ValidateComponent(componentJson, prefabName, objectIndex, componentIndex++, result);
            }
        }
    }
    if (objectJson.contains("tags") && !objectJson["tags"].is_array()) {
        result.warnings.push_back(fmt::format("{}: optional 'tags' should be an array of strings", context));
    }
}

PrefabValidationResult ValidatePrefabJson(const nlohmann::json& json, const std::filesystem::path& filePath) {
    PrefabValidationResult result;
    const std::string prefabName = filePath.stem().string();
    if (!json.is_object()) {
        result.errors.push_back(fmt::format("Prefab '{}' root must be a JSON object", filePath.string()));
        return result;
    }

    if (json.contains("gameObjects")) {
        if (!json["gameObjects"].is_array()) {
            result.errors.push_back(fmt::format("Prefab '{}': 'gameObjects' must be an array", filePath.string()));
        } else if (json["gameObjects"].empty()) {
            result.errors.push_back(fmt::format("Prefab '{}': 'gameObjects' array must contain at least one entry", filePath.string()));
        } else {
            std::size_t index = 0;
            for (const auto& objectJson : json["gameObjects"]) {
                ValidateGameObject(objectJson, prefabName, index++, result);
            }
        }
    } else {
        ValidateGameObject(json, prefabName, 0, result);
    }

    return result;
}

} // namespace

void PrefabLibrary::DispatchMessage(const std::string& message, bool isError) const {
    const bool hasCallback = static_cast<bool>(m_messageCallback);
    if (isError) {
        if (hasCallback) {
            gm::core::Logger::Debug("[PrefabLibrary] {}", message);
        } else {
            gm::core::Logger::Error("[PrefabLibrary] {}", message);
        }
    } else {
        if (hasCallback) {
            gm::core::Logger::Debug("[PrefabLibrary] {}", message);
        } else {
            gm::core::Logger::Warning("[PrefabLibrary] {}", message);
        }
    }
    if (hasCallback) {
        m_messageCallback(message, isError);
    }
}

bool PrefabLibrary::LoadDirectory(const std::filesystem::path& root) {
    if (!std::filesystem::exists(root)) {
        DispatchMessage(fmt::format("Prefab directory does not exist: {}", root.string()), false);
        return false;
    }

    bool loadedAny = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (!IsPrefabFile(entry.path())) {
            continue;
        }
        loadedAny |= LoadPrefabFile(entry.path());
    }
    return loadedAny;
}

bool PrefabLibrary::LoadPrefabFile(const std::filesystem::path& filePath) {
    try {
        nlohmann::json json;
        std::ifstream file(filePath);
        if (!file.is_open()) {
            DispatchMessage(fmt::format("Failed to open prefab file: {}", filePath.string()), true);
            return false;
        }
        file >> json;

        PrefabValidationResult validation = ValidatePrefabJson(json, filePath);
        for (const auto& warning : validation.warnings) {
            DispatchMessage(warning, false);
        }
        if (!validation.IsValid()) {
            for (const auto& error : validation.errors) {
                DispatchMessage(error, true);
            }
            return false;
        }

        PrefabDefinition def;
        def.name = filePath.stem().string();
        def.sourcePath = filePath;

        if (json.contains("name") && json["name"].is_string()) {
            def.name = json["name"].get<std::string>();
        }

        if (json.contains("gameObjects") && json["gameObjects"].is_array()) {
            for (const auto& obj : json["gameObjects"]) {
                if (obj.is_object()) {
                    def.objects.push_back(obj);
                }
            }
        } else if (json.is_object()) {
            def.objects.push_back(json);
        }

        if (def.objects.empty()) {
            DispatchMessage(fmt::format("Prefab '{}' contains no objects", filePath.string()), false);
            return false;
        }

        m_prefabs[def.name] = std::move(def);
        gm::core::Logger::Info("[PrefabLibrary] Loaded prefab '{}' from {}", def.name, filePath.string());
        return true;
    } catch (const std::exception& ex) {
        DispatchMessage(fmt::format("Failed to parse prefab '{}': {}", filePath.string(), ex.what()), true);
        return false;
    }
}

const PrefabDefinition* PrefabLibrary::GetPrefab(const std::string& name) const {
    auto it = m_prefabs.find(name);
    if (it == m_prefabs.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> PrefabLibrary::GetPrefabNames() const {
    std::vector<std::string> names;
    names.reserve(m_prefabs.size());
    for (const auto& [name, _] : m_prefabs) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::shared_ptr<gm::GameObject>> PrefabLibrary::Instantiate(
    const std::string& name,
    gm::Scene& scene,
    const glm::vec3& position,
    const glm::vec3& rotationEulerDegrees,
    const glm::vec3& scale) const {

    const PrefabDefinition* prefab = GetPrefab(name);
    if (!prefab) {
        gm::core::Logger::Warning("[PrefabLibrary] Prefab '{}' not found", name);
        return {};
    }

    glm::mat4 rootTransform = MakeTransform(position, rotationEulerDegrees, scale);

    std::vector<std::shared_ptr<gm::GameObject>> created;
    created.reserve(prefab->objects.size());

    for (const auto& objJson : prefab->objects) {
        auto obj = gm::SceneSerializer::DeserializeGameObject(scene, objJson);
        if (obj) {
            ApplyTransformToGameObject(obj, rootTransform);
            created.push_back(obj);
        }
    }

    if (created.empty()) {
        gm::core::Logger::Warning("[PrefabLibrary] Prefab '{}' produced no objects", name);
    }

    return created;
}

} // namespace gm::scene
