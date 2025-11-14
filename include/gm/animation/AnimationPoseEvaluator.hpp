#pragma once

#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/AnimationPose.hpp"
#include "gm/animation/Skeleton.hpp"

#include <vector>

namespace gm::animation {

struct AnimationLayer {
    const AnimationClip* clip = nullptr;
    double timeSeconds = 0.0;
    float weight = 1.0f;
};

class AnimationPoseEvaluator {
public:
    explicit AnimationPoseEvaluator(const Skeleton& skeleton);

    void EvaluateClip(const AnimationClip& clip,
                      double timeSeconds,
                      AnimationPose& outPose) const;

    void EvaluateLayers(const std::vector<AnimationLayer>& layers,
                        AnimationPose& outPose) const;

private:
    struct SampledTransform {
        glm::vec3 translation{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f};
        bool valid = false;
    };

    SampledTransform SampleBone(const AnimationClip& clip,
                                int boneIndex,
                                double timeSeconds) const;

    const Skeleton& m_skeleton;
};

} // namespace gm::animation


