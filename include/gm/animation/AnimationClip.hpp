#pragma once

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace gm::animation {

struct AnimationClip {
    struct VecKey {
        double time = 0.0;
        glm::vec3 value{0.0f};
    };

    struct RotKey {
        double time = 0.0;
        glm::quat value{1.0f, 0.0f, 0.0f, 0.0f};
    };

    struct Channel {
        std::string boneName;
        int boneIndex = -1;
        std::vector<VecKey> translationKeys;
        std::vector<RotKey> rotationKeys;
        std::vector<VecKey> scaleKeys;
    };

    std::string name;
    double duration = 0.0;
    double ticksPerSecond = 0.0;
    std::vector<Channel> channels;

    [[nodiscard]] bool HasBone(int boneIndex) const;

    [[nodiscard]] nlohmann::json ToJson() const;
    static AnimationClip FromJson(const nlohmann::json& json);
    static AnimationClip FromFile(const std::string& path);
    void SaveToFile(const std::string& path) const;
};

} // namespace gm::animation


