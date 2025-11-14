#include "gm/animation/SkinnedMeshAsset.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace gm::animation {

namespace {

glm::vec3 ReadVec3(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 3) {
        throw std::runtime_error("SkinnedMeshAsset vec3 must have 3 elements");
    }
    return glm::vec3(json[0].get<float>(), json[1].get<float>(), json[2].get<float>());
}

glm::vec4 ReadVec4(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 4) {
        throw std::runtime_error("SkinnedMeshAsset vec4 must have 4 elements");
    }
    return glm::vec4(json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>());
}

glm::vec2 ReadVec2(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 2) {
        throw std::runtime_error("SkinnedMeshAsset vec2 must have 2 elements");
    }
    return glm::vec2(json[0].get<float>(), json[1].get<float>());
}

nlohmann::json WriteVec3(const glm::vec3& value) {
    return nlohmann::json::array({value.x, value.y, value.z});
}

nlohmann::json WriteVec4(const glm::vec4& value) {
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

nlohmann::json WriteVec2(const glm::vec2& value) {
    return nlohmann::json::array({value.x, value.y});
}

} // namespace

nlohmann::json SkinnedMeshAsset::ToJson() const {
    nlohmann::json json;
    json["name"] = name;

    nlohmann::json verticesJson = nlohmann::json::array();
    for (const auto& vertex : vertices) {
        nlohmann::json vJson;
        vJson["position"] = WriteVec3(vertex.position);
        vJson["normal"] = WriteVec3(vertex.normal);
        vJson["tangent"] = WriteVec4(vertex.tangent);
        vJson["uv0"] = WriteVec2(vertex.uv0);

        nlohmann::json indicesJson = nlohmann::json::array();
        for (auto index : vertex.boneIndices) {
            indicesJson.push_back(index);
        }
        vJson["boneIndices"] = std::move(indicesJson);

        nlohmann::json weightsJson = nlohmann::json::array();
        for (auto weight : vertex.boneWeights) {
            weightsJson.push_back(weight);
        }
        vJson["boneWeights"] = std::move(weightsJson);

        verticesJson.push_back(std::move(vJson));
    }
    json["vertices"] = std::move(verticesJson);

    json["indices"] = indices;

    nlohmann::json sectionsJson = nlohmann::json::array();
    for (const auto& section : sections) {
        sectionsJson.push_back({
            {"materialGuid", section.materialGuid},
            {"indexOffset", section.indexOffset},
            {"indexCount", section.indexCount},
        });
    }
    json["sections"] = std::move(sectionsJson);
    json["boneNames"] = boneNames;

    return json;
}

SkinnedMeshAsset SkinnedMeshAsset::FromJson(const nlohmann::json& json) {
    SkinnedMeshAsset asset;
    asset.name = json.value("name", std::string{});

    const auto& verticesJson = json.at("vertices");
    asset.vertices.reserve(verticesJson.size());
    for (const auto& vJson : verticesJson) {
        Vertex vertex;
        vertex.position = ReadVec3(vJson.at("position"));
        vertex.normal = ReadVec3(vJson.at("normal"));
        vertex.tangent = ReadVec4(vJson.at("tangent"));
        vertex.uv0 = ReadVec2(vJson.at("uv0"));

        const auto& indicesJson = vJson.at("boneIndices");
        if (!indicesJson.is_array() || indicesJson.size() != 4) {
            throw std::runtime_error("SkinnedMeshAsset boneIndices must have 4 elements");
        }
        for (std::size_t i = 0; i < 4; ++i) {
            vertex.boneIndices[i] = indicesJson.at(i).get<std::uint16_t>();
        }

        const auto& weightsJson = vJson.at("boneWeights");
        if (!weightsJson.is_array() || weightsJson.size() != 4) {
            throw std::runtime_error("SkinnedMeshAsset boneWeights must have 4 elements");
        }
        for (std::size_t i = 0; i < 4; ++i) {
            vertex.boneWeights[i] = weightsJson.at(i).get<float>();
        }

        asset.vertices.push_back(std::move(vertex));
    }

    asset.indices = json.at("indices").get<std::vector<std::uint32_t>>();

    const auto& sectionsJson = json.at("sections");
    asset.sections.reserve(sectionsJson.size());
    for (const auto& sectionJson : sectionsJson) {
        MeshSection section;
        section.materialGuid = sectionJson.value("materialGuid", std::string{});
        section.indexOffset = sectionJson.value("indexOffset", 0u);
        section.indexCount = sectionJson.value("indexCount", 0u);
        asset.sections.push_back(section);
    }

    asset.boneNames = json.value("boneNames", std::vector<std::string>{});
    return asset;
}

SkinnedMeshAsset SkinnedMeshAsset::FromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("SkinnedMeshAsset::FromFile could not open file: " + path);
    }

    nlohmann::json json;
    file >> json;
    return FromJson(json);
}

void SkinnedMeshAsset::SaveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("SkinnedMeshAsset::SaveToFile could not open file for writing: " + path);
    }

    file << ToJson().dump(2);
}

} // namespace gm::animation


