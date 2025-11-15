#pragma once

#include "gm/scene/Component.hpp"

#include <glm/vec3.hpp>
#include <memory>
#include <string>

namespace gm {
class TransformComponent;
namespace scene {
class AnimatorComponent;
} // namespace scene
} // namespace gm

namespace gotmilked {

class CowAnimationController : public gm::Component {
public:
    CowAnimationController();

    void Init() override;
    void Update(float deltaTime) override;

    void SetIdleSlot(std::string slot) { m_idleSlot = std::move(slot); }
    void SetWalkSlot(std::string slot) { m_walkSlot = std::move(slot); }

    void SetSpeedThreshold(float threshold) { m_speedThreshold = threshold; }
    void SetBlendRate(float rate) { m_blendRate = rate; }

    [[nodiscard]] const std::string& IdleSlot() const { return m_idleSlot; }
    [[nodiscard]] const std::string& WalkSlot() const { return m_walkSlot; }
    [[nodiscard]] float SpeedThreshold() const { return m_speedThreshold; }
    [[nodiscard]] float BlendRate() const { return m_blendRate; }
    void SetTemperatureComfortRange(float minC, float maxC) { m_comfortMinC = minC; m_comfortMaxC = maxC; }
    void SetRainTolerance(float tolerance) { m_rainTolerance = tolerance; }

private:
    void ApplyWeights(float walkWeight);
    float ComputeMoodFactor() const;

    std::weak_ptr<gm::scene::AnimatorComponent> m_animator;
    std::weak_ptr<gm::TransformComponent> m_transform;

    glm::vec3 m_lastPosition{0.0f};
    bool m_hasLastPosition = false;

    float m_speedThreshold = 0.25f;
    float m_blendRate = 4.0f;
    float m_currentWalkWeight = 0.0f;

    std::string m_idleSlot = "Idle";
    std::string m_walkSlot = "Walk";
    float m_comfortMinC = 5.0f;
    float m_comfortMaxC = 25.0f;
    float m_rainTolerance = 2.0f;
};

} // namespace gotmilked

