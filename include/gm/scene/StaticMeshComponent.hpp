#pragma once

#include "gm/scene/Component.hpp"

#include <memory>

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

    // Getters
    gm::Mesh* GetMesh() const { return m_mesh; }
    gm::Shader* GetShader() const { return m_shader; }
    std::shared_ptr<gm::Material> GetMaterial() const { return m_material; }
    const gm::Camera* GetCamera() const { return m_camera; }

    void Render() override;

private:
    gm::Mesh* m_mesh = nullptr;
    gm::Shader* m_shader = nullptr;
    std::shared_ptr<gm::Material> m_material;
    const gm::Camera* m_camera = nullptr;
};

} // namespace gm::scene

