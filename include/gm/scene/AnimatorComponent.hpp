#pragma once

#include "gm/scene/Component.hpp"
#include "gm/animation/AnimationPose.hpp"
#include "gm/animation/AnimationPoseEvaluator.hpp"
#include "gm/animation/Skeleton.hpp"
#include "gm/animation/AnimationClip.hpp"
#include "gm/utils/ResourceManager.hpp"

#include <glm/mat4x4.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gm::scene {

class AnimatorComponent : public Component {
public:
    struct LayerSnapshot {
        std::string slot;
        std::string clipGuid;
        float weight = 1.0f;
        bool playing = false;
        bool loop = true;
        double timeSeconds = 0.0;
    };

    AnimatorComponent();

    void Init() override;
    void Update(float deltaTime) override;
    void OnDestroy() override;

    void SetSkeleton(std::shared_ptr<animation::Skeleton> skeleton, const std::string& guid);
    void SetSkeleton(ResourceManager::SkeletonHandle handle);
    void SetClip(const std::string& slot, std::shared_ptr<animation::AnimationClip> clip, const std::string& guid);
    void SetClip(const std::string& slot, ResourceManager::AnimationClipHandle handle);
    void Play(const std::string& slot, bool loop);
    void Stop(const std::string& slot);
    void SetWeight(const std::string& slot, float weight);

    [[nodiscard]] const animation::AnimationPose& CurrentPose() const { return m_pose; }
    [[nodiscard]] const std::string& SkeletonGuid() const { return m_skeletonGuid; }
    [[nodiscard]] std::shared_ptr<animation::Skeleton> GetSkeletonAsset() const { return m_skeleton; }

    bool GetSkinningPalette(std::vector<glm::mat4>& outPalette);
    bool GetBoneModelMatrices(std::vector<glm::mat4>& outMatrices);
    std::vector<LayerSnapshot> GetLayerSnapshots() const;
    void ApplyLayerSnapshot(const LayerSnapshot& snapshot);

private:
    struct LayerState {
        std::shared_ptr<animation::AnimationClip> clip;
        std::string clipGuid;
        float weight = 1.0f;
        bool playing = false;
        bool loop = true;
        double timeSeconds = 0.0;
        ResourceManager::AnimationClipHandle handle;
    };

    std::shared_ptr<animation::Skeleton> m_skeleton;
    std::string m_skeletonGuid;
    std::unique_ptr<animation::AnimationPoseEvaluator> m_evaluator;
    ResourceManager::SkeletonHandle m_skeletonHandle;

    std::unordered_map<std::string, LayerState> m_layers;
    animation::AnimationPose m_pose;
    bool m_paletteDirty = true;
    std::vector<glm::mat4> m_skinningPalette;
    std::vector<glm::mat4> m_globalMatrices;

    bool EnsurePoseCache();
};

} // namespace gm::scene

