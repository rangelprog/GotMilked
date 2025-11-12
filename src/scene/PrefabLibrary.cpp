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
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace gm::scene {

namespace {

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

} // namespace

bool PrefabLibrary::LoadDirectory(const std::filesystem::path& root) {
    if (!std::filesystem::exists(root)) {
        gm::core::Logger::Warning("[PrefabLibrary] Prefab directory does not exist: {}", root.string());
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
            gm::core::Logger::Error("[PrefabLibrary] Failed to open prefab file: {}", filePath.string());
            return false;
        }
        file >> json;

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
            gm::core::Logger::Warning("[PrefabLibrary] Prefab '{}' contains no objects", filePath.string());
            return false;
        }

        m_prefabs[def.name] = std::move(def);
        gm::core::Logger::Info("[PrefabLibrary] Loaded prefab '{}' from {}", def.name, filePath.string());
        return true;
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[PrefabLibrary] Failed to parse prefab '{}': {}", filePath.string(), ex.what());
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
