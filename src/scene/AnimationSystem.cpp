#include "gm/scene/AnimationSystem.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/AnimatorComponent.hpp"
#include "gm/core/Logger.hpp"

namespace gm::scene {

void AnimationSystem::Update(Scene& scene, float deltaTime) {
    auto& objects = scene.GetAllGameObjects();
    for (auto& object : objects) {
        if (!object || object->IsDestroyed()) {
            continue;
        }
        if (auto animator = object->GetComponent<AnimatorComponent>()) {
            animator->Update(deltaTime);
        }
    }
}

} // namespace gm::scene


