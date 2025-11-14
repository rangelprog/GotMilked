#include "gm/animation/AnimationPoseEvaluator.hpp"

#include "gm/animation/PoseMath.hpp"

#include <algorithm>
#include <unordered_map>

namespace gm::animation {

namespace {

double ClampTime(double t, double duration) {
    if (duration <= 0.0) {
        return 0.0;
    }
    if (t < 0.0) {
        t = std::fmod(t, duration);
        if (t < 0.0) {
            t += duration;
        }
        return t;
    }
    return std::fmod(t, duration);
}

template <typename KeyType>
std::size_t FindKeyIndex(const std::vector<KeyType>& keys, double time) {
    if (keys.empty() || time <= keys.front().time) {
        return 0;
    }
    for (std::size_t i = 0; i + 1 < keys.size(); ++i) {
        if (time < keys[i + 1].time) {
            return i;
        }
    }
    return keys.size() - 1;
}

glm::vec3 SampleTranslation(const AnimationClip::Channel& channel, double timeTicks) {
    if (channel.translationKeys.empty()) {
        return glm::vec3(0.0f);
    }

    const auto& keys = channel.translationKeys;
    const std::size_t index = FindKeyIndex(keys, timeTicks);
    if (index + 1 >= keys.size()) {
        return keys.back().value;
    }

    const auto& a = keys[index];
    const auto& b = keys[index + 1];
    const double span = b.time - a.time;
    float t = (span <= 0.0) ? 0.0f : static_cast<float>((timeTicks - a.time) / span);
    return pose::LerpVec3(a.value, b.value, t);
}

glm::quat SampleRotation(const AnimationClip::Channel& channel, double timeTicks) {
    if (channel.rotationKeys.empty()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    const auto& keys = channel.rotationKeys;
    const std::size_t index = FindKeyIndex(keys, timeTicks);
    if (index + 1 >= keys.size()) {
        return glm::normalize(keys.back().value);
    }

    const auto& a = keys[index];
    const auto& b = keys[index + 1];
    const double span = b.time - a.time;
    float t = (span <= 0.0) ? 0.0f : static_cast<float>((timeTicks - a.time) / span);
    glm::quat qa = glm::normalize(a.value);
    glm::quat qb = glm::normalize(b.value);
    pose::AlignHemisphere(qb, qa);
    return pose::SlerpQuat(qa, qb, t);
}

glm::vec3 SampleScale(const AnimationClip::Channel& channel, double timeTicks) {
    if (channel.scaleKeys.empty()) {
        return glm::vec3(1.0f);
    }

    const auto& keys = channel.scaleKeys;
    const std::size_t index = FindKeyIndex(keys, timeTicks);
    if (index + 1 >= keys.size()) {
        return keys.back().value;
    }

    const auto& a = keys[index];
    const auto& b = keys[index + 1];
    const double span = b.time - a.time;
    float t = (span <= 0.0) ? 0.0f : static_cast<float>((timeTicks - a.time) / span);
    return pose::LerpVec3(a.value, b.value, t);
}

} // namespace

AnimationPoseEvaluator::AnimationPoseEvaluator(const Skeleton& skeleton)
    : m_skeleton(skeleton) {}

AnimationPoseEvaluator::SampledTransform AnimationPoseEvaluator::SampleBone(
    const AnimationClip& clip,
    int boneIndex,
    double timeSeconds) const {

    const double ticks = (clip.ticksPerSecond > 0.0)
                             ? timeSeconds * clip.ticksPerSecond
                             : timeSeconds;

    const double wrapped = ClampTime(ticks, clip.duration);

    for (const auto& channel : clip.channels) {
        if (channel.boneIndex != boneIndex) {
            continue;
        }

        SampledTransform result;
        result.translation = SampleTranslation(channel, wrapped);
        result.rotation = SampleRotation(channel, wrapped);
        result.scale = SampleScale(channel, wrapped);
        result.valid = true;
        return result;
    }

    return {};
}

void AnimationPoseEvaluator::EvaluateClip(const AnimationClip& clip,
                                          double timeSeconds,
                                          AnimationPose& outPose) const {
    const std::size_t boneCount = m_skeleton.bones.size();
    outPose.Resize(boneCount);

    for (std::size_t i = 0; i < boneCount; ++i) {
        auto sample = SampleBone(clip, static_cast<int>(i), timeSeconds);
        auto& transform = outPose.LocalTransform(i);

        if (sample.valid) {
            transform.translation = sample.translation;
            transform.rotation = sample.rotation;
            transform.scale = sample.scale;
        } else {
            transform.translation = glm::vec3(0.0f);
            transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            transform.scale = glm::vec3(1.0f);
        }
    }

    outPose.BuildLocalMatrices();
}

void AnimationPoseEvaluator::EvaluateLayers(const std::vector<AnimationLayer>& layers,
                                            AnimationPose& outPose) const {
    const std::size_t boneCount = m_skeleton.bones.size();
    outPose.Resize(boneCount);

    std::vector<glm::vec3> accumTranslation(boneCount, glm::vec3(0.0f));
    std::vector<glm::vec4> accumRotation(boneCount, glm::vec4(0.0f));
    std::vector<glm::vec3> accumScale(boneCount, glm::vec3(0.0f));
    std::vector<float> accumWeight(boneCount, 0.0f);

    for (const auto& layer : layers) {
        if (!layer.clip || layer.weight <= 0.0f) {
            continue;
        }

        const double time = layer.timeSeconds;
        for (std::size_t i = 0; i < boneCount; ++i) {
            auto sample = SampleBone(*layer.clip, static_cast<int>(i), time);
            if (!sample.valid) {
                continue;
            }

            const float w = layer.weight;
            accumTranslation[i] += sample.translation * w;

            glm::quat rotation = sample.rotation;
            if (accumWeight[i] > 0.0f) {
                glm::quat reference(accumRotation[i].w,
                                    accumRotation[i].x,
                                    accumRotation[i].y,
                                    accumRotation[i].z);
                pose::AlignHemisphere(rotation, reference);
            }
            accumRotation[i] += glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w) * w;
            accumScale[i] += sample.scale * w;
            accumWeight[i] += w;
        }
    }

    for (std::size_t i = 0; i < boneCount; ++i) {
        auto& transform = outPose.LocalTransform(i);
        const float weight = accumWeight[i];
        if (weight <= 0.0f) {
            transform.translation = glm::vec3(0.0f);
            transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            transform.scale = glm::vec3(1.0f);
            continue;
        }

        transform.translation = accumTranslation[i] / weight;
        glm::vec4 rotation = accumRotation[i] / weight;
        transform.rotation = glm::normalize(glm::quat(rotation.w, rotation.x, rotation.y, rotation.z));

        transform.scale = accumScale[i] / weight;
    }

    outPose.BuildLocalMatrices();
}

} // namespace gm::animation


