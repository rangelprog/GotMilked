#include "SandboxSceneHelpers.hpp"

#include "SandboxResources.hpp"
#include "MeshSpinnerComponent.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/MaterialComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/rendering/Material.hpp"

#include <cstdio>

namespace sandbox {

void PopulateSandboxScene(
    gm::Scene& scene,
    gm::Camera& camera,
    const SandboxResources& resources,
    std::vector<std::shared_ptr<gm::GameObject>>& spinnerObjects)
{
    spinnerObjects.clear();

    const int numSpinners = 3;
    const float spacing = 3.0f;

    for (int i = 0; i < numSpinners; ++i) {
        std::string objectName = "Spinner_" + std::to_string(i + 1);

        auto spinnerObject = scene.SpawnGameObject(objectName);
        scene.TagGameObject(spinnerObject, "spinner");
        scene.TagGameObject(spinnerObject, "demo");

        auto transform = spinnerObject->EnsureTransform();
        float xPos = (i - (numSpinners - 1) / 2.0f) * spacing;
        transform->SetPosition(xPos, 0.0f, -5.0f);
        transform->SetScale(1.0f);

        auto material = std::make_shared<gm::Material>();
        material->SetName("Spinner Material " + std::to_string(i + 1));
        if (resources.texture) {
            material->SetDiffuseTexture(resources.texture.get());
        }
        material->SetDiffuseColor(glm::vec3(1.0f, 1.0f, 1.0f));
        material->SetSpecularColor(glm::vec3(0.3f, 0.3f, 0.3f));
        material->SetShininess(32.0f);

        auto materialComp = spinnerObject->AddComponent<gm::MaterialComponent>();
        materialComp->SetMaterial(material);

        auto spinner = spinnerObject->AddComponent<MeshSpinnerComponent>();
        if (resources.mesh) {
            spinner->SetMesh(resources.mesh.get());
        }
        if (resources.texture) {
            spinner->SetTexture(resources.texture.get());
        }
        if (resources.shader) {
            spinner->SetShader(resources.shader.get());
        }
        spinner->SetCamera(&camera);
        spinner->SetMeshGuid(resources.meshGuid);
        spinner->SetMeshPath(resources.meshPath);
        spinner->SetTextureGuid(resources.textureGuid);
        spinner->SetTexturePath(resources.texturePath);
        spinner->SetShaderGuid(resources.shaderGuid);
        spinner->SetShaderPaths(resources.shaderVertPath, resources.shaderFragPath);
        spinner->SetRotationSpeed(15.0f + i * 5.0f);
        spinner->Init();

        std::printf("[Game] %s created at position (%.1f, 0.0, -5.0)\n",
                    objectName.c_str(), xPos);

        spinnerObjects.push_back(spinnerObject);
    }

    auto sunLight = scene.CreateGameObject("Sun");
    auto sunTransform = sunLight->EnsureTransform();
    sunTransform->SetPosition(0.0f, 10.0f, 0.0f);
    auto sunLightComp = sunLight->AddComponent<gm::LightComponent>();
    sunLightComp->SetType(gm::LightComponent::LightType::Directional);
    sunLightComp->SetDirection(glm::vec3(-0.4f, -1.0f, -0.3f));
    sunLightComp->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    sunLightComp->SetIntensity(1.5f);
    std::printf("[Game] Created directional light (Sun)\n");

    auto spinners = scene.FindGameObjectsByTag("spinner");
    std::printf("[Game] Found %zu spinner objects in scene\n", spinners.size());
    std::printf("[Game] Scene setup complete with %zu spinners and 1 light\n", spinnerObjects.size());
}

void RehydrateMeshSpinnerComponents(
    gm::Scene& scene,
    const SandboxResources& resources,
    gm::Camera* camera)
{
    for (auto& obj : scene.GetAllGameObjects()) {
        if (!obj) continue;
        auto spinner = obj->GetComponent<MeshSpinnerComponent>();
        if (!spinner) continue;

        if (resources.mesh &&
            ((!spinner->GetMeshGuid().empty() && spinner->GetMeshGuid() == resources.meshGuid) ||
             (!spinner->GetMeshPath().empty() && spinner->GetMeshPath() == resources.meshPath)))
        {
            spinner->SetMesh(resources.mesh.get());
        }

        if (resources.texture &&
            ((!spinner->GetTextureGuid().empty() && spinner->GetTextureGuid() == resources.textureGuid) ||
             (!spinner->GetTexturePath().empty() && spinner->GetTexturePath() == resources.texturePath)))
        {
            spinner->SetTexture(resources.texture.get());
        }

        if (resources.shader &&
            ((!spinner->GetShaderGuid().empty() && spinner->GetShaderGuid() == resources.shaderGuid) ||
             (!spinner->GetShaderVertPath().empty() && spinner->GetShaderVertPath() == resources.shaderVertPath)))
        {
            spinner->SetShader(resources.shader.get());
        }

        spinner->SetCamera(camera);
    }
}

void CollectMeshSpinnerObjects(
    gm::Scene& scene,
    std::vector<std::shared_ptr<gm::GameObject>>& outObjects)
{
    outObjects.clear();
    for (auto& obj : scene.GetAllGameObjects()) {
        if (obj && obj->HasComponent<MeshSpinnerComponent>()) {
            outObjects.push_back(obj);
        }
    }
}

} // namespace sandbox

