#pragma once

#include <functional>

namespace gm {
class Scene;
class Camera;
}

struct GameResources;
struct GLFWwindow;

namespace gotmilked {

void PopulateInitialScene(
    gm::Scene& scene,
    gm::Camera& camera,
    const GameResources& resources,
    GLFWwindow* window = nullptr,
    std::function<float()> fovProvider = {});

} // namespace gotmilked

