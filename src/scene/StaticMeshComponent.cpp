#include "gm/scene/StaticMeshComponent.hpp"

#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/core/Logger.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace gm::scene {

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

void StaticMeshComponent::RestoreResources(
    std::function<gm::Mesh*(const std::string&)> meshResolver,
    std::function<gm::Shader*(const std::string&)> shaderResolver,
    std::function<std::shared_ptr<gm::Material>(const std::string&)> materialResolver) {
    
    std::string ownerName = GetOwner() ? GetOwner()->GetName() : "unknown";
    bool allResolved = true;
    
    // Restore mesh
    if (!m_meshGuid.empty()) {
        if (meshResolver) {
            gm::Mesh* mesh = meshResolver(m_meshGuid);
            if (mesh) {
                m_mesh = mesh;
                core::Logger::Debug(
                    "[StaticMeshComponent] Successfully restored mesh from GUID '%s' for GameObject '%s'",
                    m_meshGuid.c_str(), ownerName.c_str());
            } else {
                core::Logger::Warning(
                    "[StaticMeshComponent] Failed to resolve mesh GUID '%s' for GameObject '%s'. "
                    "Component will not render correctly.",
                    m_meshGuid.c_str(), ownerName.c_str());
                allResolved = false;
            }
        } else {
            core::Logger::Warning(
                "[StaticMeshComponent] Mesh resolver not provided for GUID '%s' on GameObject '%s'",
                m_meshGuid.c_str(), ownerName.c_str());
            allResolved = false;
        }
    } else if (!m_mesh) {
        core::Logger::Debug(
            "[StaticMeshComponent] No mesh GUID or mesh set for GameObject '%s'",
            ownerName.c_str());
    }
    
    // Restore shader
    if (!m_shaderGuid.empty()) {
        if (shaderResolver) {
            gm::Shader* shader = shaderResolver(m_shaderGuid);
            if (shader) {
                m_shader = shader;
                core::Logger::Debug(
                    "[StaticMeshComponent] Successfully restored shader from GUID '%s' for GameObject '%s'",
                    m_shaderGuid.c_str(), ownerName.c_str());
            } else {
                core::Logger::Warning(
                    "[StaticMeshComponent] Failed to resolve shader GUID '%s' for GameObject '%s'. "
                    "Component will not render correctly.",
                    m_shaderGuid.c_str(), ownerName.c_str());
                allResolved = false;
            }
        } else {
            core::Logger::Warning(
                "[StaticMeshComponent] Shader resolver not provided for GUID '%s' on GameObject '%s'",
                m_shaderGuid.c_str(), ownerName.c_str());
            allResolved = false;
        }
    } else if (!m_shader) {
        core::Logger::Debug(
            "[StaticMeshComponent] No shader GUID or shader set for GameObject '%s'",
            ownerName.c_str());
    }
    
    // Restore material (optional)
    if (!m_materialGuid.empty()) {
        if (materialResolver) {
            auto material = materialResolver(m_materialGuid);
            if (material) {
                m_material = material;
                core::Logger::Debug(
                    "[StaticMeshComponent] Successfully restored material from GUID '%s' for GameObject '%s'",
                    m_materialGuid.c_str(), ownerName.c_str());
            } else {
                core::Logger::Warning(
                    "[StaticMeshComponent] Failed to resolve material GUID '%s' for GameObject '%s'. "
                    "Component will render without material.",
                    m_materialGuid.c_str(), ownerName.c_str());
                // Material is optional, so don't mark as failed
            }
        } else {
            core::Logger::Debug(
                "[StaticMeshComponent] Material resolver not provided for GUID '%s' on GameObject '%s' "
                "(material is optional)",
                m_materialGuid.c_str(), ownerName.c_str());
        }
    }
    
    // Validate that required resources are present
    if (!m_mesh || !m_shader) {
        core::Logger::Warning(
            "[StaticMeshComponent] GameObject '%s' is missing required resources after restoration "
            "(mesh: %s, shader: %s). Component will not render.",
            ownerName.c_str(),
            m_mesh ? "present" : "missing",
            m_shader ? "present" : "missing");
    } else if (allResolved) {
        core::Logger::Debug(
            "[StaticMeshComponent] Successfully restored all resources for GameObject '%s'",
            ownerName.c_str());
    }
}

} // namespace gm::scene

