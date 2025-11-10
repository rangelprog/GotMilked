#include "Scene.hpp"
#include <glm/gtc/matrix_transform.hpp>

void Scene::draw(Shader &shader, const Camera &cam, int fbw, int fbh,
                 float fovDeg) {
  if (fbw <= 0 || fbh <= 0)
    return;

  const float aspect = (float)fbw / (float)fbh;
  glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 100.0f);
  glm::mat4 view = cam.view();

  shader.use();

  for (const auto &e : entities) {
    glm::mat4 model = e.transform.toMat4();
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

    shader.setMat4("uModel", model);
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);
    shader.setMat3("uNormalMat", normalMat);

    if (e.texture) {
      shader.setInt("uUseTex", 1);
      e.texture->bind(0);
    } else {
      shader.setInt("uUseTex", 0);
    }

    e.mesh->draw();
  }
}
