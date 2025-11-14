#pragma once

#include "gm/scene/SceneSystem.hpp"

namespace gm::scene {

class AnimationSystem : public SceneSystem {
public:
    static constexpr std::string_view kName = "AnimationSystem";

    std::string_view GetName() const override { return kName; }
    void Update(Scene& scene, float deltaTime) override;
};

} // namespace gm::scene


