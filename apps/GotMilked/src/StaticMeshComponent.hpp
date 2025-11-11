#pragma once

#include "gm/scene/Component.hpp"

#include <memory>

namespace gm {
class Mesh;
class Shader;
class Material;
class Camera;
class TransformComponent;
}

class StaticMeshComponent : public gm::Component {
public:
    StaticMeshComponent();

    void SetMesh(gm::Mesh* mesh) { m_mesh = mesh; }
    void SetShader(gm::Shader* shader) { m_shader = shader; }
    void SetMaterial(std::shared_ptr<gm::Material> material) { m_material = std::move(material); }
    void SetCamera(const gm::Camera* camera) { m_camera = camera; }

    void Render() override;

private:
    gm::Mesh* m_mesh = nullptr;
    gm::Shader* m_shader = nullptr;
    std::shared_ptr<gm::Material> m_material;
    const gm::Camera* m_camera = nullptr;
};


