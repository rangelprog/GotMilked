#include "gm/animation/PoseMath.hpp"

#include <glm/gtx/quaternion.hpp>

namespace gm::animation::pose {

glm::vec3 LerpVec3(const glm::vec3& a, const glm::vec3& b, float t) {
    return glm::mix(a, b, t);
}

glm::quat SlerpQuat(const glm::quat& a, const glm::quat& b, float t) {
    return glm::normalize(glm::slerp(a, b, t));
}

glm::vec3 MultiplyVec3(const glm::vec3& a, const glm::vec3& b) {
    return glm::vec3(a.x * b.x, a.y * b.y, a.z * b.z);
}

void AlignHemisphere(glm::quat& target, const glm::quat& reference) {
    if (glm::dot(target, reference) < 0.0f) {
        target = -target;
    }
}

} // namespace gm::animation::pose


