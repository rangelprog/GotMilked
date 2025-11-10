#pragma once
#include <gm/Mesh.hpp>
#include <gm/Texture.hpp>
#include <gm/Transform.hpp>
#include <glm/glm.hpp>

namespace gm {

struct SceneEntity {
    Mesh* mesh = nullptr;
    Texture* texture = nullptr;
    Transform transform;
};

} // namespace gm
