#pragma once

#if GM_DEBUG_TOOLS

#include "gm/scene/Component.hpp"
#include "GameConstants.hpp"

#include <functional>
#include <memory>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct GLFWwindow;

namespace gm {
class Mesh;
class Shader;
class Material;
class Camera;
class TransformComponent;
}

namespace gm::debug {

class EditableTerrainComponent : public gm::Component {
public:
    EditableTerrainComponent();

    void Init() override;
    void Update(float deltaTime) override;
    void Render() override;

    void SetCamera(gm::Camera* camera) { m_camera = camera; }
    void SetShader(gm::Shader* shader) { m_shader = shader; }
    void SetMaterial(std::shared_ptr<gm::Material> material) { m_material = std::move(material); }
    void SetWindow(GLFWwindow* window) { m_window = window; }
    void SetTerrainSize(float sizeMeters);
    void SetResolution(int resolution);
    void SetFovProvider(std::function<float()> provider) { m_fovProvider = std::move(provider); }

    bool IsEditingEnabled() const { return m_editingEnabled; }
    void SetEditingEnabled(bool enabled) { m_editingEnabled = enabled; }
    void SetEditorWindowVisible(bool visible) { m_editorWindowVisible = visible; }
    bool IsEditorWindowVisible() const { return m_editorWindowVisible; }

    int GetResolution() const { return m_resolution; }
    float GetTerrainSize() const { return m_size; }
    float GetMinHeight() const { return m_minHeight; }
    void SetMinHeight(float height);
    float GetMaxHeight() const { return m_maxHeight; }
    void SetMaxHeight(float height);
    float GetBrushRadius() const { return m_brushRadius; }
    void SetBrushRadius(float radius);
    float GetBrushStrength() const { return m_brushStrength; }
    void SetBrushStrength(float strength);
    const std::vector<float>& GetHeights() const { return m_heights; }

    bool SetHeightData(int resolution,
                       float size,
                       float minHeight,
                       float maxHeight,
                       const std::vector<float>& heights);

    // Force mesh rebuild (useful after loading scenes)
    void MarkMeshDirty() { m_meshDirty = true; }

private:
    void InitializeHeightmap();
    void ResampleHeightmap(int newResolution);
    bool RebuildMesh();
    void BuildIndexBuffer();
    void ApplyBrush(const glm::vec2& localXZ, float deltaTime, float direction);
    bool ComputeTerrainHit(glm::vec3& outWorldPos, glm::vec2& outLocalXZ) const;

    GLFWwindow* m_window = nullptr;
    gm::Camera* m_camera = nullptr;
    gm::Shader* m_shader = nullptr;
    std::shared_ptr<gm::Material> m_material;
    std::unique_ptr<gm::Mesh> m_mesh;

    int m_resolution = gotmilked::GameConstants::Terrain::DefaultResolution;
    float m_size = gotmilked::GameConstants::Terrain::DefaultSize;
    float m_minHeight = gotmilked::GameConstants::Terrain::DefaultMinHeight;
    float m_maxHeight = gotmilked::GameConstants::Terrain::DefaultMaxHeight;
    float m_brushRadius = gotmilked::GameConstants::Terrain::DefaultBrushRadius;
    float m_brushStrength = gotmilked::GameConstants::Terrain::DefaultBrushStrength;

    bool m_editingEnabled = false;
    bool m_editorWindowVisible = false;
    bool m_meshDirty = false;

    std::vector<float> m_heights;
    std::vector<unsigned int> m_indices;

    std::function<float()> m_fovProvider;
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
