#include "CowRendererComponent.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

CowRendererComponent::CowRendererComponent()
    : gm::Component() {
    name = "CowRenderer";
}

void CowRendererComponent::Init() {
    // Component initialization - mesh, texture, shader should already be set
    if (!mesh || !texture || !shader || !camera) {
        // Log warning but don't fail - these can be set later
    }
}

void CowRendererComponent::Update(float deltaTime) {
    // Update elapsed time for rotation animation
    elapsedTime += deltaTime;
    
    // Apply rotation to TransformComponent if present
    if (owner) {
        if (auto transform = owner->GetTransform()) {
            transform->SetRotation(0.0f, elapsedTime * rotationSpeed, 0.0f);
        }
    }
}

void CowRendererComponent::Render() {
    if (!mesh || !texture || !shader || !camera || !owner) {
        return;
    }

    // Get model matrix from TransformComponent, or use identity if not present
    glm::mat4 model = glm::mat4(1.0f);
    if (auto transform = owner->GetTransform()) {
        model = transform->GetMatrix();
    }
    
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

    shader->Use();
    shader->SetMat4("uModel", model);
    shader->SetMat4("uView", camera->View());
    shader->SetMat4("uProj", projMatrix);

    if (GLint loc = shader->uniformLoc("uNormalMat"); loc >= 0)
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(normalMat));

    if (GLint loc = shader->uniformLoc("uViewPos"); loc >= 0)
        glUniform3fv(loc, 1, glm::value_ptr(camera->Position()));

    if (GLint loc = shader->uniformLoc("uLightDir"); loc >= 0)
        glUniform3fv(loc, 1, glm::value_ptr(glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f))));
    if (GLint loc = shader->uniformLoc("uLightColor"); loc >= 0)
        glUniform3fv(loc, 1, glm::value_ptr(glm::vec3(1.0f)));

    if (GLint loc = shader->uniformLoc("uUseTex"); loc >= 0)
        glUniform1i(loc, 1);
    
    texture->bind(0);
    mesh->Draw();
}
