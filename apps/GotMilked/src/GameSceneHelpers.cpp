#include "GameSceneHelpers.hpp"

#include "GameResources.hpp"
#include "GameConstants.hpp"
#if GM_DEBUG_TOOLS
#include "EditableTerrainComponent.hpp"
#endif

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/core/Logger.hpp"

#include <glm/vec3.hpp>
#include <utility>

namespace gotmilked {

#if GM_DEBUG_TOOLS
using gm::debug::EditableTerrainComponent;
#endif

void PopulateInitialScene(
    gm::Scene& scene,
    gm::Camera& camera,
    const GameResources& resources,
    GLFWwindow* window,
    std::function<float()> fovProvider)
{
    // Directional light
    auto sunLight = scene.CreateGameObject("Sun");
    auto sunTransform = sunLight->EnsureTransform();
    sunTransform->SetPosition(GameConstants::Light::SunPosition);
    auto sunLightComp = sunLight->AddComponent<gm::LightComponent>();
    sunLightComp->SetType(gm::LightComponent::LightType::Directional);
    sunLightComp->SetDirection(GameConstants::Light::SunDirection);
    sunLightComp->SetColor(GameConstants::Light::SunColor);
    sunLightComp->SetIntensity(GameConstants::Light::SunIntensity);
    sunLight->AddTag("lighting");

    // Editable terrain
    auto terrainObject = scene.SpawnGameObject("Terrain");
    scene.TagGameObject(terrainObject, "terrain");
    auto terrainTransform = terrainObject->EnsureTransform();
    terrainTransform->SetPosition(GameConstants::Transform::Origin);
    terrainTransform->SetScale(GameConstants::Transform::UnitScale);

#if GM_DEBUG_TOOLS
    auto terrainComponent = terrainObject->AddComponent<EditableTerrainComponent>();
    terrainComponent->SetCamera(&camera);
    if (resources.GetShader()) {
        terrainComponent->SetShader(resources.GetShader());
    }
    if (resources.GetTerrainMaterial()) {
        terrainComponent->SetMaterial(resources.GetTerrainMaterial());
    }
    terrainComponent->SetWindow(window);
    if (fovProvider) {
        terrainComponent->SetFovProvider(std::move(fovProvider));
    }
    terrainComponent->SetTerrainSize(GameConstants::Terrain::InitialSize);
#else
    (void)resources;
    (void)window;
    (void)fovProvider;
#endif

    gm::core::Logger::Info("[Game] Scene populated with editable terrain");
}

} // namespace gotmilked

