#pragma once
#include "Mesh.hpp"
#include "Texture.hpp"
#include "Transform.hpp"
#include <glm/glm.hpp>

struct SceneEntity {
  Mesh *mesh = nullptr;
  Texture *texture = nullptr;
  Transform transform;
};
