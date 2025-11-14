#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace gm::animation {

struct SkinnedMeshAsset {
    struct Vertex {
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f};
        glm::vec4 tangent{0.0f};
        glm::vec2 uv0{0.0f};
        std::array<std::uint16_t, 4> boneIndices{0, 0, 0, 0};
        std::array<float, 4> boneWeights{0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct MeshSection {
        std::string materialGuid;
        std::uint32_t indexOffset = 0;
        std::uint32_t indexCount = 0;
    };

    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<MeshSection> sections;
    std::vector<std::string> boneNames;

    [[nodiscard]] nlohmann::json ToJson() const;
    static SkinnedMeshAsset FromJson(const nlohmann::json& json);
    static SkinnedMeshAsset FromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;
};

} // namespace gm::animation


