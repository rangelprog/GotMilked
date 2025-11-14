#include "gm/animation/Skeleton.hpp"

#include <nlohmann/json.hpp>

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <stdexcept>

namespace gm::animation {

int Skeleton::FindBoneIndex(std::string_view boneName) const {
    for (std::size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].name == boneName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const Skeleton::Bone* Skeleton::FindBone(std::string_view boneName) const {
    const int index = FindBoneIndex(boneName);
    if (index < 0 || static_cast<std::size_t>(index) >= bones.size()) {
        return nullptr;
    }
    return &bones[static_cast<std::size_t>(index)];
}

nlohmann::json Skeleton::ToJson() const {
    nlohmann::json json;
    json["name"] = name;
    nlohmann::json boneArray = nlohmann::json::array();

    for (const auto& bone : bones) {
        nlohmann::json boneJson;
        boneJson["name"] = bone.name;
        boneJson["parent"] = bone.parentIndex;
        const float* data = glm::value_ptr(bone.inverseBindMatrix);
        boneJson["inverseBindMatrix"] = std::vector<float>(data, data + 16);
        boneArray.push_back(std::move(boneJson));
    }

    json["bones"] = std::move(boneArray);
    return json;
}

Skeleton Skeleton::FromJson(const nlohmann::json& json) {
    Skeleton skeleton;
    skeleton.name = json.value("name", std::string{});

    const auto& boneArray = json.at("bones");
    skeleton.bones.reserve(boneArray.size());
    for (const auto& boneJson : boneArray) {
        Bone bone;
        bone.name = boneJson.at("name").get<std::string>();
        bone.parentIndex = boneJson.value("parent", -1);

        const auto& matrixData = boneJson.at("inverseBindMatrix");
        if (!matrixData.is_array() || matrixData.size() != 16) {
            throw std::runtime_error("Skeleton::FromJson expected 16 floats for inverse bind matrix");
        }

        float values[16];
        for (std::size_t i = 0; i < 16; ++i) {
            values[i] = matrixData.at(i).get<float>();
        }
        bone.inverseBindMatrix = glm::make_mat4(values);
        skeleton.bones.push_back(std::move(bone));
    }

    return skeleton;
}

Skeleton Skeleton::FromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Skeleton::FromFile could not open file: " + path);
    }

    nlohmann::json json;
    file >> json;
    return FromJson(json);
}

void Skeleton::SaveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Skeleton::SaveToFile could not open file for writing: " + path);
    }

    const auto json = ToJson();
    file << json.dump(2);
}

} // namespace gm::animation


