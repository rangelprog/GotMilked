#include "GameSceneHelpers.hpp"

#include "GameResources.hpp"
#include "GameConstants.hpp"
#if GM_DEBUG_TOOLS
#include "EditableTerrainComponent.hpp"
#endif

#include "gm/gameplay/CameraRigComponent.hpp"
#include "gm/gameplay/QuestTriggerComponent.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/Component.hpp"
#include "gm/core/Logger.hpp"

#include <glm/vec3.hpp>
#include <utility>

namespace gotmilked {

#if GM_DEBUG_TOOLS
using gm::debug::EditableTerrainComponent;
#endif
using gm::gameplay::CameraRigComponent;

void PopulateInitialScene(
    gm::Scene& scene,
    gm::Camera& camera,
    const GameResources& resources,
    GLFWwindow* window,
    std::function<float()> fovProvider)
{
    (void)camera;
    (void)fovProvider;

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

    auto cameraRigObject = scene.CreateGameObject("CameraRig");
    auto cameraRigComponent = cameraRigObject->AddComponent<CameraRigComponent>();
    cameraRigComponent->SetRigId("PrimaryCamera");
    cameraRigComponent->SetInitialFov(GameConstants::Camera::DefaultFovDegrees);

#if GM_DEBUG_TOOLS
    auto terrainComponent = terrainObject->AddComponent<EditableTerrainComponent>();
    if (resources.GetShader()) {
        terrainComponent->SetShader(resources.GetShader());
    }
    if (resources.GetTerrainMaterial()) {
        terrainComponent->SetMaterial(resources.GetTerrainMaterial());
    }
    terrainComponent->SetTerrainSize(GameConstants::Terrain::InitialSize);
    if (!resources.GetTextureGuid().empty() && resources.GetTexture()) {
        terrainComponent->SetBaseTexture(resources.GetTextureGuid(), resources.GetTexture());
    }
    terrainComponent->FillPaintLayer(0.0f);
#else
    (void)resources;
#endif
    (void)window;

    gm::core::Logger::Info("[Game] Scene populated with editable terrain");
}

void PopulateSmoketestScene(
    gm::Scene& scene,
    gm::Camera& camera,
    GameResources& resources,
    GLFWwindow* window,
    std::function<float()> fovProvider)
{
    PopulateInitialScene(scene, camera, resources, window, std::move(fovProvider));

    auto createNpc = [&](const std::string& name, const glm::vec3& position, const std::string& questId) {
        auto npc = scene.SpawnGameObject(name);
        auto transform = npc->EnsureTransform();
        transform->SetPosition(position);
        transform->SetScale(glm::vec3(1.0f));
        npc->AddTag("npc");
        auto questComponent = npc->AddComponent<gm::gameplay::QuestTriggerComponent>();
        questComponent->SetQuestId(questId);
        questComponent->SetActivationRadius(2.5f);
        questComponent->SetTriggerOnSceneLoad(false);
        questComponent->SetTriggerOnInteract(true);
    };

    createNpc("QuestGiver_A", {2.0f, 0.0f, 4.0f}, "quest_deliver_milk");
    createNpc("QuestGiver_B", {-3.0f, 0.0f, -2.0f}, "quest_fix_tractor");

    auto createVehicle = [&](const std::string& name, const glm::vec3& position, const glm::vec3& scale) {
        auto vehicle = scene.SpawnGameObject(name);
        auto transform = vehicle->EnsureTransform();
        transform->SetPosition(position);
        transform->SetScale(scale);
        vehicle->AddTag("vehicle");
        if (resources.GetMesh()) {
            auto meshComp = vehicle->AddComponent<gm::scene::StaticMeshComponent>();
            meshComp->SetMesh(resources.GetMesh(), resources.GetMeshGuid());
            meshComp->SetShader(resources.GetShader(), resources.GetShaderGuid());
        }
    };

    createVehicle("BarnTruck", {6.0f, 0.0f, 1.5f}, {1.2f, 1.2f, 1.2f});
    createVehicle("FieldTractor", {-5.0f, 0.0f, 5.0f}, {1.0f, 1.0f, 1.0f});

#if GM_DEBUG_TOOLS
    auto terrain = scene.FindGameObjectByName("Terrain");
    if (terrain) {
        if (auto editable = terrain->GetComponent<EditableTerrainComponent>()) {
            editable->SetTerrainSize(GameConstants::Terrain::InitialSize * 0.75f);
        }
    }
#endif
}

} // namespace gotmilked

