#pragma once
#include "gm/scene/Component.hpp"
#include <memory>

namespace gm {
class Material;
}

namespace gm {

/**
 * @brief Component that holds a material for rendering
 * 
 * Allows GameObjects to have material properties.
 * The material can be shared across multiple objects.
 */
class MaterialComponent : public Component {
public:
    MaterialComponent();
    ~MaterialComponent() override = default;

    void Init() override {}
    void Update(float deltaTime) override {}
    void Render() override {}

    // Material management
    void SetMaterial(std::shared_ptr<Material> material) { m_material = material; }
    std::shared_ptr<Material> GetMaterial() const { return m_material; }
    bool HasMaterial() const { return m_material != nullptr; }

private:
    std::shared_ptr<Material> m_material;
};

} // namespace gm

