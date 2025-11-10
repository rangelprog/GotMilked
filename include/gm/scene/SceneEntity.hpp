#pragma once
#include <gm/rendering/Mesh.hpp>
#include <gm/rendering/Texture.hpp>
#include <gm/scene/Transform.hpp>
#include <glm/glm.hpp>

namespace gm {

struct SceneEntity {
    Mesh* mesh = nullptr;
    Texture* texture = nullptr;
    Transform transform;
};

} // namespace gm
