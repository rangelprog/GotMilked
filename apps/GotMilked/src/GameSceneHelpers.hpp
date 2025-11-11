#pragma once

namespace gm {
class Scene;
class Camera;
}

struct GameResources;

namespace gotmilked {

void PopulateInitialScene(
    gm::Scene& scene,
    gm::Camera& camera,
    const GameResources& resources);

} // namespace gotmilked

