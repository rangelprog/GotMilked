#include "MeshSpinnerComponent.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/MaterialComponent.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

MeshSpinnerComponent::MeshSpinnerComponent()
    : gm::Component() {
    name = "MeshSpinnerComponent";
}

void MeshSpinnerComponent::Init() {
    // Component initialization - mesh, texture, shader should already be set
    if (!mesh || !texture || !shader || !camera) {
        // Log warning but don't fail - these can be set later
    }
}

void MeshSpinnerComponent::Update(float deltaTime) {
    if (!owner) {
        return;
    }

    if (auto transform = owner->GetTransform()) {
        transform->Rotate(0.0f, rotationSpeed * deltaTime, 0.0f);
    }
}

void MeshSpinnerComponent::Render() {
    if (!mesh || !shader || !camera || !owner) {
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

    // Lights are now handled by LightManager in Scene::Draw()
    // Legacy light uniforms removed - use LightComponent instead

    // Apply material if MaterialComponent exists, otherwise use fallback
    if (auto materialComp = owner->GetComponent<gm::MaterialComponent>()) {
        if (auto material = materialComp->GetMaterial()) {
            material->Apply(*shader);
        } else {
            // Fallback: use texture if available
            if (texture) {
                shader->SetInt("uUseTex", 1);
                texture->bind(0);
                shader->SetInt("uTex", 0);
            } else {
                shader->SetInt("uUseTex", 0);
                shader->SetVec3("uSolidColor", glm::vec3(0.8f));
            }
        }
    } else {
        // Legacy: use texture directly if no material component
        if (texture) {
            shader->SetInt("uUseTex", 1);
            texture->bind(0);
            shader->SetInt("uTex", 0);
        } else {
            shader->SetInt("uUseTex", 0);
            shader->SetVec3("uSolidColor", glm::vec3(0.8f));
        }
    }
    
    mesh->Draw();
}
