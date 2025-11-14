#include "gm/animation/AnimationClip.hpp"

#include <nlohmann/json.hpp>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <stdexcept>

namespace gm::animation {

namespace {

glm::vec3 ReadVec3(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 3) {
        throw std::runtime_error("AnimationClip json vec3 must have 3 elements");
    }
    return glm::vec3(json[0].get<float>(), json[1].get<float>(), json[2].get<float>());
}

glm::quat ReadQuat(const nlohmann::json& json) {
    if (!json.is_array() || json.size() != 4) {
        throw std::runtime_error("AnimationClip json quat must have 4 elements");
    }
    return glm::quat(json[0].get<float>(), json[1].get<float>(), json[2].get<float>(), json[3].get<float>());
}

nlohmann::json WriteVec3(const glm::vec3& value) {
    return nlohmann::json::array({value.x, value.y, value.z});
}

nlohmann::json WriteQuat(const glm::quat& value) {
    return nlohmann::json::array({value.w, value.x, value.y, value.z});
}

} // namespace

bool AnimationClip::HasBone(int boneIndex) const {
    if (boneIndex < 0) {
        return false;
    }
    for (const auto& channel : channels) {
        if (channel.boneIndex == boneIndex) {
            return true;
        }
    }
    return false;
}

nlohmann::json AnimationClip::ToJson() const {
    nlohmann::json json;
    json["name"] = name;
    json["duration"] = duration;
    json["ticksPerSecond"] = ticksPerSecond;

    nlohmann::json channelArray = nlohmann::json::array();

    for (const auto& channel : channels) {
        nlohmann::json channelJson;
        channelJson["boneName"] = channel.boneName;
        channelJson["boneIndex"] = channel.boneIndex;

        nlohmann::json translationArray = nlohmann::json::array();
        for (const auto& key : channel.translationKeys) {
            translationArray.push_back({{"time", key.time}, {"value", WriteVec3(key.value)}});
        }

        nlohmann::json rotationArray = nlohmann::json::array();
        for (const auto& key : channel.rotationKeys) {
            rotationArray.push_back({{"time", key.time}, {"value", WriteQuat(key.value)}});
        }

        nlohmann::json scaleArray = nlohmann::json::array();
        for (const auto& key : channel.scaleKeys) {
            scaleArray.push_back({{"time", key.time}, {"value", WriteVec3(key.value)}});
        }

        channelJson["translation"] = std::move(translationArray);
        channelJson["rotation"] = std::move(rotationArray);
        channelJson["scale"] = std::move(scaleArray);

        channelArray.push_back(std::move(channelJson));
    }

    json["channels"] = std::move(channelArray);
    return json;
}

AnimationClip AnimationClip::FromJson(const nlohmann::json& json) {
    AnimationClip clip;
    clip.name = json.value("name", std::string{});
    clip.duration = json.value("duration", 0.0);
    clip.ticksPerSecond = json.value("ticksPerSecond", 0.0);

    const auto& channelArray = json.at("channels");
    clip.channels.reserve(channelArray.size());

    for (const auto& channelJson : channelArray) {
        Channel channel;
        channel.boneName = channelJson.value("boneName", std::string{});
        channel.boneIndex = channelJson.value("boneIndex", -1);

        if (const auto& translationArray = channelJson.at("translation"); translationArray.is_array()) {
            channel.translationKeys.reserve(translationArray.size());
            for (const auto& keyJson : translationArray) {
                VecKey key;
                key.time = keyJson.at("time").get<double>();
                key.value = ReadVec3(keyJson.at("value"));
                channel.translationKeys.push_back(key);
            }
        }

        if (const auto& rotationArray = channelJson.at("rotation"); rotationArray.is_array()) {
            channel.rotationKeys.reserve(rotationArray.size());
            for (const auto& keyJson : rotationArray) {
                RotKey key;
                key.time = keyJson.at("time").get<double>();
                key.value = ReadQuat(keyJson.at("value"));
                channel.rotationKeys.push_back(key);
            }
        }

        if (const auto& scaleArray = channelJson.at("scale"); scaleArray.is_array()) {
            channel.scaleKeys.reserve(scaleArray.size());
            for (const auto& keyJson : scaleArray) {
                VecKey key;
                key.time = keyJson.at("time").get<double>();
                key.value = ReadVec3(keyJson.at("value"));
                channel.scaleKeys.push_back(key);
            }
        }

        clip.channels.push_back(std::move(channel));
    }

    return clip;
}

AnimationClip AnimationClip::FromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("AnimationClip::FromFile could not open file: " + path);
    }

    nlohmann::json json;
    file >> json;
    return FromJson(json);
}

void AnimationClip::SaveToFile(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("AnimationClip::SaveToFile could not open file for writing: " + path);
    }

    file << ToJson().dump(2);
}

} // namespace gm::animation


