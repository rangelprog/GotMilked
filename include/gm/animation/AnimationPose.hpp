#pragma once

#include "gm/animation/Skeleton.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace gm::animation {

struct BoneTransform {
    glm::vec3 translation{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
};

class AnimationPose {
public:
    AnimationPose() = default;
    explicit AnimationPose(std::size_t boneCount);

    void Resize(std::size_t boneCount);
    [[nodiscard]] std::size_t Size() const { return m_localTransforms.size(); }

    BoneTransform& LocalTransform(std::size_t index);
    const BoneTransform& LocalTransform(std::size_t index) const;

    const std::vector<BoneTransform>& LocalTransforms() const { return m_localTransforms; }
    std::vector<BoneTransform>& LocalTransforms() { return m_localTransforms; }

    [[nodiscard]] const std::vector<glm::mat4>& LocalMatrices() const { return m_localMatrices; }
    void BuildLocalMatrices();

private:
    std::vector<BoneTransform> m_localTransforms;
    std::vector<glm::mat4> m_localMatrices;
};

} // namespace gm::animation


