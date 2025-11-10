#pragma once
#include "Camera.hpp"
#include "SceneEntity.hpp"
#include "Shader.hpp"
#include <vector>

class Scene {
public:
  void add(const SceneEntity &e) { entities.push_back(e); }

  void draw(Shader &shader, const Camera &cam, int fbw, int fbh, float fovDeg);

private:
  std::vector<SceneEntity> entities;
};
