#pragma once

#include <glm/mat4x4.hpp>

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace gm::animation {

struct Skeleton {
    struct Bone {
        std::string name;
        int parentIndex = -1;
        glm::mat4 inverseBindMatrix{1.0f};
    };

    std::string name;
    std::vector<Bone> bones;

    [[nodiscard]] int FindBoneIndex(std::string_view boneName) const;
    [[nodiscard]] const Bone* FindBone(std::string_view boneName) const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static Skeleton FromJson(const nlohmann::json& json);
    static Skeleton FromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;
};

} // namespace gm::animation


