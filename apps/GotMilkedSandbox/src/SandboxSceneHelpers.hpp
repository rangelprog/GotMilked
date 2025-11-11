#pragma once

#include <memory>
#include <vector>

namespace gm {
class Scene;
class Camera;
class GameObject;
}

struct SandboxResources;

namespace sandbox {

void PopulateSandboxScene(
    gm::Scene& scene,
    gm::Camera& camera,
    const SandboxResources& resources,
    std::vector<std::shared_ptr<gm::GameObject>>& spinnerObjects);

void RehydrateMeshSpinnerComponents(
    gm::Scene& scene,
    const SandboxResources& resources,
    gm::Camera* camera);

void CollectMeshSpinnerObjects(
    gm::Scene& scene,
    std::vector<std::shared_ptr<gm::GameObject>>& outObjects);

} // namespace sandbox

