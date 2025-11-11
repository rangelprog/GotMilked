#include "StaticMeshComponent.hpp"

#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

StaticMeshComponent::StaticMeshComponent() {
    SetName("StaticMeshComponent");
}

void StaticMeshComponent::Render() {
    if (!m_mesh || !m_shader || !GetOwner()) {
        return;
    }

    auto transform = GetOwner()->GetTransform();
    if (!transform) {
        transform = GetOwner()->EnsureTransform();
    }

    const glm::mat4 model = transform->GetMatrix();
    const glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

    m_shader->Use();
    m_shader->SetMat4("uModel", model);
    m_shader->SetMat3("uNormalMat", normalMat);

    if (m_camera) {
        m_shader->SetVec3("uViewPos", m_camera->Position());
    }

    if (m_material) {
        m_material->Apply(*m_shader);
    }

    m_mesh->Draw();
}


