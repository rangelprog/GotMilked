#pragma once

#include "gm/scene/Component.hpp"

#include <memory>
#include <string>
#include <functional>

namespace gm {
class Mesh;
class Shader;
class Material;
class Camera;
namespace scene {
class TransformComponent;
}
}

namespace gm::scene {

/**
 * @brief Component for rendering a static mesh with shader and material
 */
class StaticMeshComponent : public Component {
public:
    StaticMeshComponent();

    void SetMesh(gm::Mesh* mesh) { m_mesh = mesh; }
    void SetShader(gm::Shader* shader) { m_shader = shader; }
    void SetMaterial(std::shared_ptr<gm::Material> material) { m_material = std::move(material); }
    void SetCamera(const gm::Camera* camera) { m_camera = camera; }

    // Setters with GUIDs (for serialization)
    void SetMesh(gm::Mesh* mesh, const std::string& guid) { 
        m_mesh = mesh; 
        m_meshGuid = guid;
    }
    void SetShader(gm::Shader* shader, const std::string& guid) { 
        m_shader = shader; 
        m_shaderGuid = guid;
    }
    void SetMaterial(std::shared_ptr<gm::Material> material, const std::string& guid) { 
        m_material = std::move(material); 
        m_materialGuid = guid;
    }

    // Getters
    gm::Mesh* GetMesh() const { return m_mesh; }
    gm::Shader* GetShader() const { return m_shader; }
    std::shared_ptr<gm::Material> GetMaterial() const { return m_material; }
    const gm::Camera* GetCamera() const { return m_camera; }

    // GUID getters
    const std::string& GetMeshGuid() const { return m_meshGuid; }
    const std::string& GetShaderGuid() const { return m_shaderGuid; }
    const std::string& GetMaterialGuid() const { return m_materialGuid; }

    /**
     * @brief Restore resources from GUIDs using resolver functions
     * @param meshResolver Function that resolves mesh GUID to Mesh pointer
     * @param shaderResolver Function that resolves shader GUID to Shader pointer
     * @param materialResolver Function that resolves material GUID to Material shared_ptr
     */
    void RestoreResources(
        std::function<gm::Mesh*(const std::string&)> meshResolver,
        std::function<gm::Shader*(const std::string&)> shaderResolver,
        std::function<std::shared_ptr<gm::Material>(const std::string&)> materialResolver);

    void Render() override;

private:
    gm::Mesh* m_mesh = nullptr;
    gm::Shader* m_shader = nullptr;
    std::shared_ptr<gm::Material> m_material;
    const gm::Camera* m_camera = nullptr;

    // Resource GUIDs for serialization
    std::string m_meshGuid;
    std::string m_shaderGuid;
    std::string m_materialGuid;
};

} // namespace gm::scene

