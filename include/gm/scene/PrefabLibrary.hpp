#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>

namespace gm {
class Scene;
class GameObject;
}

namespace gm::scene {

struct PrefabDefinition {
    std::string name;
    std::filesystem::path sourcePath;
    std::vector<nlohmann::json> objects;
};

class PrefabLibrary {
public:
    bool LoadDirectory(const std::filesystem::path& root);
    bool LoadPrefabFile(const std::filesystem::path& filePath);
    void SetMessageCallback(std::function<void(const std::string&, bool isError)> callback) { m_messageCallback = std::move(callback); }

    const PrefabDefinition* GetPrefab(const std::string& name) const;
    std::vector<std::string> GetPrefabNames() const;

    std::vector<std::shared_ptr<gm::GameObject>> Instantiate(
        const std::string& name,
        gm::Scene& scene,
        const glm::vec3& position = glm::vec3(0.0f),
        const glm::vec3& rotationEulerDegrees = glm::vec3(0.0f),
        const glm::vec3& scale = glm::vec3(1.0f)) const;

private:
    std::unordered_map<std::string, PrefabDefinition> m_prefabs;
    std::function<void(const std::string&, bool)> m_messageCallback;

    void DispatchMessage(const std::string& message, bool isError) const;
};

} // namespace gm::scene
