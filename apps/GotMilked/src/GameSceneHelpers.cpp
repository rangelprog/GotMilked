#include "GameSceneHelpers.hpp"

#include "GameResources.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/LightComponent.hpp"

#include <cstdio>

namespace gotmilked {

void PopulateInitialScene(
    gm::Scene& scene,
    gm::Camera& /* camera */,
    const GameResources& /* resources */)
{
    // Create a directional light (sun)
    auto sunLight = scene.CreateGameObject("Sun");
    auto sunTransform = sunLight->EnsureTransform();
    sunTransform->SetPosition(0.0f, 10.0f, 0.0f);
    auto sunLightComp = sunLight->AddComponent<gm::LightComponent>();
    sunLightComp->SetType(gm::LightComponent::LightType::Directional);
    sunLightComp->SetDirection(glm::vec3(-0.4f, -1.0f, -0.3f));
    sunLightComp->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    sunLightComp->SetIntensity(1.5f);
    std::printf("[Game] Created directional light (Sun)\n");
    std::printf("[Game] Scene setup complete with 1 light\n");
}

} // namespace gotmilked

