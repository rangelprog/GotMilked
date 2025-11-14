#include "gm/animation/AnimationPose.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace gm::animation {

namespace {
glm::mat4 ToMatrix(const BoneTransform& transform) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, transform.translation);
    model *= glm::mat4_cast(transform.rotation);
    model = glm::scale(model, transform.scale);
    return model;
}
} // namespace

AnimationPose::AnimationPose(std::size_t boneCount) {
    Resize(boneCount);
}

void AnimationPose::Resize(std::size_t boneCount) {
    m_localTransforms.resize(boneCount);
    m_localMatrices.resize(boneCount, glm::mat4(1.0f));
}

BoneTransform& AnimationPose::LocalTransform(std::size_t index) {
    return m_localTransforms[index];
}

const BoneTransform& AnimationPose::LocalTransform(std::size_t index) const {
    return m_localTransforms[index];
}

void AnimationPose::BuildLocalMatrices() {
    if (m_localMatrices.size() != m_localTransforms.size()) {
        m_localMatrices.resize(m_localTransforms.size());
    }
    for (std::size_t i = 0; i < m_localTransforms.size(); ++i) {
        m_localMatrices[i] = ToMatrix(m_localTransforms[i]);
    }
}

} // namespace gm::animation


