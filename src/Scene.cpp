#include "gm/Scene.hpp"
#include "gm/Shader.hpp"
#include "gm/Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace gm {

void Scene::Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
    if (fbw <= 0 || fbh <= 0)
        return;

    const float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
    glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 100.0f);
    glm::mat4 view = cam.View();

    shader.Use();

    for (const auto& entity : entities) {
        if (!entity) continue;

        glm::mat4 model = entity->transform.getMatrix();
        glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

        shader.SetMat4("uModel", model);
        shader.SetMat4("uView", view);
        shader.SetMat4("uProj", proj);
        shader.SetMat3("uNormalMat", normalMat);

        if (entity->texture) {
            shader.SetInt("uUseTex", 1);
            entity->texture->bind(0);
        } else {
            shader.SetInt("uUseTex", 0);
        }

        if (entity->mesh) {
            entity->mesh->Draw();
        }
    }
}

} // namespace gm
