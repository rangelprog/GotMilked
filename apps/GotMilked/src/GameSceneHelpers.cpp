#include "GameSceneHelpers.hpp"

#include "GameResources.hpp"
#include "EditableTerrainComponent.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/LightComponent.hpp"

#include <cstdio>
#include <glm/vec3.hpp>
#include <utility>

namespace gotmilked {

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
    sunTransform->SetPosition(0.0f, 10.0f, 0.0f);
    auto sunLightComp = sunLight->AddComponent<gm::LightComponent>();
    sunLightComp->SetType(gm::LightComponent::LightType::Directional);
    sunLightComp->SetDirection(glm::vec3(-0.4f, -1.0f, -0.3f));
    sunLightComp->SetColor(glm::vec3(1.0f));
    sunLightComp->SetIntensity(1.5f);
    sunLight->AddTag("lighting");

    // Editable terrain
    auto terrainObject = scene.SpawnGameObject("Terrain");
    scene.TagGameObject(terrainObject, "terrain");
    auto terrainTransform = terrainObject->EnsureTransform();
    terrainTransform->SetPosition(0.0f, 0.0f, 0.0f);
    terrainTransform->SetScale(glm::vec3(1.0f));

    auto terrainComponent = terrainObject->AddComponent<EditableTerrainComponent>();
    terrainComponent->SetCamera(&camera);
    if (resources.shader) {
        terrainComponent->SetShader(resources.shader.get());
    }
    if (resources.planeMaterial) {
        terrainComponent->SetMaterial(resources.planeMaterial);
    }
    terrainComponent->SetWindow(window);
    if (fovProvider) {
        terrainComponent->SetFovProvider(std::move(fovProvider));
    }
    terrainComponent->SetTerrainSize(40.0f);

    std::printf("[Game] Scene populated with editable terrain\n");
}

} // namespace gotmilked

