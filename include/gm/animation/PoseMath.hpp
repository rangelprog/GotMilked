#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>

namespace gm::animation::pose {

[[nodiscard]] glm::vec3 LerpVec3(const glm::vec3& a, const glm::vec3& b, float t);
[[nodiscard]] glm::quat SlerpQuat(const glm::quat& a, const glm::quat& b, float t);
[[nodiscard]] glm::vec3 MultiplyVec3(const glm::vec3& a, const glm::vec3& b);
void AlignHemisphere(glm::quat& target, const glm::quat& reference);

} // namespace gm::animation::pose


