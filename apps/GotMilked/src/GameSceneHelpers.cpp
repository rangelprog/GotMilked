#include "GameSceneHelpers.hpp"

#include "GameResources.hpp"
#include "StaticMeshComponent.hpp"
#include "RigidBodyComponent.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/LightComponent.hpp"

#include <cstdio>
#include <glm/vec3.hpp>

namespace gotmilked {

namespace {

void ConfigureStaticRenderer(const std::shared_ptr<gm::GameObject>& object,
                             gm::Mesh* mesh,
                             const std::shared_ptr<gm::Material>& material,
                             gm::Shader* shader,
                             const gm::Camera& camera) {
    if (!object || !mesh || !shader || !material) {
        return;
    }

    auto renderer = object->AddComponent<StaticMeshComponent>();
    renderer->SetMesh(mesh);
    renderer->SetShader(shader);
    renderer->SetMaterial(material);
    renderer->SetCamera(&camera);
}

} // namespace

void PopulateInitialScene(
    gm::Scene& scene,
    gm::Camera& camera,
    const GameResources& resources)
{
    // Directional light
    auto sunLight = scene.CreateGameObject("Sun");
    auto sunTransform = sunLight->EnsureTransform();
    sunTransform->SetPosition(0.0f, 10.0f, 0.0f);
    auto sunLightComp = sunLight->AddComponent<gm::LightComponent>();
    sunLightComp->SetType(gm::LightComponent::LightType::Directional);
    sunLightComp->SetDirection(glm::vec3(-0.4f, -1.0f, -0.3f));
    sunLightComp->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    sunLightComp->SetIntensity(1.5f);
    sunLight->AddTag("lighting");

    // Ground plane
    auto ground = scene.SpawnGameObject("GroundPlane");
    ground->AddTag("ground");
    auto groundTransform = ground->EnsureTransform();
    groundTransform->SetPosition(0.0f, 0.0f, 0.0f);
    groundTransform->SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    ConfigureStaticRenderer(ground,
                            resources.planeMesh.get(),
                            resources.planeMaterial,
                            resources.shader.get(),
                            camera);
    
    // Add physics component for ground plane
    auto groundPhysics = ground->AddComponent<RigidBodyComponent>();
    groundPhysics->SetBodyType(RigidBodyComponent::BodyType::Static);
    groundPhysics->SetColliderShape(RigidBodyComponent::ColliderShape::Plane);
    groundPhysics->SetPlaneNormal(glm::vec3(0.0f, 1.0f, 0.0f));
    groundPhysics->SetPlaneConstant(0.0f);
    groundPhysics->Init();

    // Floating cube
    auto cube = scene.SpawnGameObject("FloatingCube");
    cube->AddTag("dynamic");
    auto cubeTransform = cube->EnsureTransform();
    cubeTransform->SetPosition(0.0f, 10.0f, 0.0f);
    cubeTransform->SetScale(glm::vec3(1.0f));
    ConfigureStaticRenderer(cube,
                            resources.cubeMesh.get(),
                            resources.cubeMaterial,
                            resources.shader.get(),
                            camera);
    
    // Add physics component for floating cube
    auto cubePhysics = cube->AddComponent<RigidBodyComponent>();
    cubePhysics->SetBodyType(RigidBodyComponent::BodyType::Dynamic);
    cubePhysics->SetColliderShape(RigidBodyComponent::ColliderShape::Box);
    cubePhysics->SetBoxHalfExtent(glm::vec3(0.75f));
    cubePhysics->SetMass(5.0f);
    cubePhysics->Init();

    std::printf("[Game] Scene populated with ground plane and floating cube\n");
}

} // namespace gotmilked

