#include "gm/scene/AnimatorComponent.hpp"

#include "gm/core/Logger.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <algorithm>

namespace gm::scene {
namespace {
void ResetPoseToIdentity(animation::AnimationPose& pose) {
    const std::size_t count = pose.Size();
    for (std::size_t i = 0; i < count; ++i) {
        auto& transform = pose.LocalTransform(i);
        transform.translation = glm::vec3(0.0f);
        transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        transform.scale = glm::vec3(1.0f);
    }
    pose.BuildLocalMatrices();
}
} // namespace

AnimatorComponent::AnimatorComponent() {
    SetName("AnimatorComponent");
}

void AnimatorComponent::Init() {
    if (m_skeletonHandle.IsValid()) {
        m_skeleton = m_skeletonHandle.Lock();
    }

    const std::size_t boneCount = m_skeleton ? m_skeleton->bones.size() : 0;
    m_pose.Resize(boneCount);

    if (m_skeleton) {
        m_evaluator = std::make_unique<animation::AnimationPoseEvaluator>(*m_skeleton);
    }

    ResetPoseToIdentity(m_pose);
    m_paletteDirty = true;
}

void AnimatorComponent::Update(float deltaTime) {
    if (m_skeletonHandle.IsValid()) {
        auto refreshedSkeleton = m_skeletonHandle.Lock();
        if (refreshedSkeleton && refreshedSkeleton != m_skeleton) {
            m_skeleton = refreshedSkeleton;
            const std::size_t boneCount = m_skeleton->bones.size();
            m_pose.Resize(boneCount);
            m_evaluator = std::make_unique<animation::AnimationPoseEvaluator>(*m_skeleton);
            ResetPoseToIdentity(m_pose);
            m_paletteDirty = true;
        }
    }

    if (!m_skeleton || !m_evaluator) {
        return;
    }

    std::vector<animation::AnimationLayer> layers;
    layers.reserve(m_layers.size());

    for (auto& [slot, layer] : m_layers) {
        if (layer.handle.IsValid()) {
            layer.clip = layer.handle.Lock();
        }

        if (!layer.clip || !layer.playing || layer.weight <= 0.0f) {
            continue;
        }

        layer.timeSeconds += deltaTime;
        const double duration = (layer.clip->ticksPerSecond > 0.0)
                                    ? (layer.clip->duration / layer.clip->ticksPerSecond)
                                    : layer.clip->duration;
        if (!layer.loop && duration > 0.0 && layer.timeSeconds >= duration) {
            layer.playing = false;
            continue;
        }

        layers.push_back(animation::AnimationLayer{
            layer.clip.get(),
            layer.timeSeconds,
            layer.weight
        });
    }

    const std::size_t expectedBoneCount = m_skeleton ? m_skeleton->bones.size() : 0;
    if (m_pose.Size() != expectedBoneCount) {
        m_pose.Resize(expectedBoneCount);
        ResetPoseToIdentity(m_pose);
        m_paletteDirty = true;
    }

    if (!layers.empty()) {
        m_evaluator->EvaluateLayers(layers, m_pose);
        m_paletteDirty = true;
    } else {
        ResetPoseToIdentity(m_pose);
        m_paletteDirty = true;
    }
}

void AnimatorComponent::OnDestroy() {
    m_layers.clear();
    m_evaluator.reset();
    m_skeleton.reset();
    m_skeletonHandle.Reset();
    m_skinningPalette.clear();
    m_globalMatrices.clear();
    m_paletteDirty = true;
}

void AnimatorComponent::SetSkeleton(std::shared_ptr<animation::Skeleton> skeleton, const std::string& guid) {
    m_skeleton = std::move(skeleton);
    m_skeletonGuid = guid;
    const std::size_t boneCount = m_skeleton ? m_skeleton->bones.size() : 0;
    m_pose.Resize(boneCount);
    if (m_skeleton) {
        m_evaluator = std::make_unique<animation::AnimationPoseEvaluator>(*m_skeleton);
    } else {
        m_evaluator.reset();
    }
    m_skeletonHandle.Reset();
    ResetPoseToIdentity(m_pose);
    m_paletteDirty = true;
}

void AnimatorComponent::SetSkeleton(ResourceManager::SkeletonHandle handle) {
    m_skeletonHandle = std::move(handle);
    m_skeletonGuid = m_skeletonHandle.Guid();
    m_skeleton = m_skeletonHandle.Lock();
    const std::size_t boneCount = m_skeleton ? m_skeleton->bones.size() : 0;
    m_pose.Resize(boneCount);
    if (m_skeleton) {
        m_evaluator = std::make_unique<animation::AnimationPoseEvaluator>(*m_skeleton);
    } else {
        m_evaluator.reset();
    }
    ResetPoseToIdentity(m_pose);
    m_paletteDirty = true;
}

void AnimatorComponent::SetClip(const std::string& slot,
                                std::shared_ptr<animation::AnimationClip> clip,
                                const std::string& guid) {
    auto& layer = m_layers[slot];
    layer.clip = std::move(clip);
    layer.clipGuid = guid;
    layer.timeSeconds = 0.0;
    layer.handle.Reset();
    m_paletteDirty = true;
}

void AnimatorComponent::SetClip(const std::string& slot, ResourceManager::AnimationClipHandle handle) {
    auto& layer = m_layers[slot];
    layer.handle = std::move(handle);
    layer.clipGuid = layer.handle.Guid();
    layer.clip = layer.handle.Lock();
    layer.timeSeconds = 0.0;
    m_paletteDirty = true;
}

void AnimatorComponent::Play(const std::string& slot, bool loop) {
    auto it = m_layers.find(slot);
    if (it == m_layers.end()) {
        gm::core::Logger::Warning("[AnimatorComponent] Attempted to play unknown slot '{}'", slot);
        return;
    }
    it->second.playing = true;
    it->second.loop = loop;
    m_paletteDirty = true;
}

void AnimatorComponent::Stop(const std::string& slot) {
    auto it = m_layers.find(slot);
    if (it != m_layers.end()) {
        it->second.playing = false;
        m_paletteDirty = true;
    }
}

void AnimatorComponent::SetWeight(const std::string& slot, float weight) {
    auto it = m_layers.find(slot);
    if (it != m_layers.end()) {
        it->second.weight = std::max(0.0f, weight);
        m_paletteDirty = true;
    }
}

bool AnimatorComponent::GetSkinningPalette(std::vector<glm::mat4>& outPalette) {
    if (!EnsurePoseCache()) {
        outPalette.clear();
        return false;
    }

    outPalette = m_skinningPalette;
    return true;
}

bool AnimatorComponent::GetBoneModelMatrices(std::vector<glm::mat4>& outMatrices) {
    if (!EnsurePoseCache()) {
        outMatrices.clear();
        return false;
    }

    outMatrices = m_globalMatrices;
    return true;
}

std::vector<AnimatorComponent::LayerSnapshot> AnimatorComponent::GetLayerSnapshots() const {
    std::vector<LayerSnapshot> snapshots;
    snapshots.reserve(m_layers.size());
    for (const auto& [slot, layer] : m_layers) {
        LayerSnapshot snapshot;
        snapshot.slot = slot;
        snapshot.clipGuid = layer.clipGuid;
        snapshot.weight = layer.weight;
        snapshot.playing = layer.playing;
        snapshot.loop = layer.loop;
        snapshot.timeSeconds = layer.timeSeconds;
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
}

void AnimatorComponent::ApplyLayerSnapshot(const LayerSnapshot& snapshot) {
    auto& layer = m_layers[snapshot.slot];
    layer.clipGuid = snapshot.clipGuid;
    layer.clip.reset();
    layer.handle.Reset();
    layer.weight = snapshot.weight;
    layer.playing = snapshot.playing;
    layer.loop = snapshot.loop;
    layer.timeSeconds = snapshot.timeSeconds;
    m_paletteDirty = true;
}

bool AnimatorComponent::EnsurePoseCache() {
    if (!m_skeleton || m_pose.Size() != m_skeleton->bones.size()) {
        return false;
    }

    if (!m_paletteDirty) {
        return true;
    }

    const std::size_t boneCount = m_skeleton->bones.size();
    if (m_skinningPalette.size() != boneCount) {
        m_skinningPalette.resize(boneCount);
    }
    if (m_globalMatrices.size() != boneCount) {
        m_globalMatrices.resize(boneCount);
    }

    m_pose.BuildLocalMatrices();
    const auto& locals = m_pose.LocalMatrices();

    for (std::size_t i = 0; i < boneCount; ++i) {
        glm::mat4 global = locals[i];
        const int parent = m_skeleton->bones[i].parentIndex;
        if (parent >= 0) {
            const std::size_t parentIndex = static_cast<std::size_t>(parent);
            if (parentIndex < m_globalMatrices.size()) {
                global = m_globalMatrices[parentIndex] * global;
            }
        }
        m_globalMatrices[i] = global;
        m_skinningPalette[i] = global * m_skeleton->bones[i].inverseBindMatrix;
    }

    m_paletteDirty = false;
    return true;
}

} // namespace gm::scene