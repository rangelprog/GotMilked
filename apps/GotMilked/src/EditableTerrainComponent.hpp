#pragma once

#if GM_DEBUG_TOOLS

#include "gm/scene/Component.hpp"
#include "GameConstants.hpp"

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct GLFWwindow;

namespace gm {
class Mesh;
class Shader;
class Material;
class Texture;
class Camera;
class TransformComponent;
}

namespace gm::debug {

class EditableTerrainComponent : public gm::Component {
public:
    enum class BrushMode {
        Sculpt = 0,
        Paint
    };

    static constexpr int kMaxPaintLayers = 4;

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
    BrushMode GetBrushMode() const { return m_brushMode; }
    void SetBrushMode(BrushMode mode) { m_brushMode = mode; }
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

    void SetTextureTiling(float tiling);
    float GetTextureTiling() const { return m_textureTiling; }

    void SetBaseTexture(const std::string& guid, gm::Texture* texture);
    void ClearBaseTexture();
    const std::string& GetBaseTextureGuid() const { return m_baseTextureGuid; }
    gm::Texture* GetBaseTexture() const { return m_baseTexture; }
    void SetBaseTextureGuidFromSave(const std::string& guid);
    void BindBaseTexture(gm::Texture* texture);

    int GetPaintLayerCount() const { return m_paintLayerCount; }
    int GetActivePaintLayerIndex() const { return m_activePaintLayer; }
    void SetActivePaintLayerIndex(int index);
    bool CanAddPaintLayer() const { return m_paintLayerCount < kMaxPaintLayers; }
    bool AddPaintLayer();
    void SetPaintLayerCount(int count);

    void SetPaintTexture(const std::string& guid, gm::Texture* texture);
    void ClearPaintTexture();
    const std::string& GetPaintTextureGuid() const;
    gm::Texture* GetPaintTexture() const;
    const std::string& GetPaintTextureGuid(int layer) const;
    gm::Texture* GetPaintTexture(int layer) const;
    bool PaintLayerHasTexture(int layer) const;
    bool PaintLayerHasPaint(int layer) const;
    const std::vector<float>& GetPaintLayerWeights(int layer) const;

    bool IsPaintLayerEnabled(int layer) const;
    void SetPaintLayerEnabled(int layer, bool enabled);
    bool RemovePaintLayer(int layer);
    void BindPaintTexture(int layer, gm::Texture* texture);
    void SetPaintLayerData(int layer, const std::string& guid, bool enabled, const std::vector<float>& weights);

    void FillPaintLayer(float weight);

    // Force mesh rebuild (useful after loading scenes)
    void MarkMeshDirty() { m_meshDirty = true; }

private:
    struct PaintLayer {
        std::string guid;
        gm::Texture* texture = nullptr;
        std::vector<float> weights;
        bool hasPaint = false;
        bool enabled = true;
    };

    void InitializeHeightmap();
    void ResampleHeightmap(int newResolution);
    void ResamplePaintLayers(int oldResolution, int newResolution,
                             const std::array<std::vector<float>, kMaxPaintLayers>& oldWeights);
    bool RebuildMesh();
    void BuildIndexBuffer();
    void ApplyHeightBrush(const glm::vec2& localXZ, float deltaTime, float direction);
    void ApplyTextureBrush(const glm::vec2& localXZ, float deltaTime, float direction);
    void UpdatePaintLayerState();
    void EnsureLayerWeightsSize(std::vector<float>& weights);
    void ClearAllPaintWeights();
    bool ComputeTerrainHit(glm::vec3& outWorldPos, glm::vec2& outLocalXZ) const;

    GLFWwindow* m_window = nullptr;
    gm::Camera* m_camera = nullptr;
    gm::Shader* m_shader = nullptr;
    std::shared_ptr<gm::Material> m_material;
    std::unique_ptr<gm::Mesh> m_mesh;
    gm::Texture* m_baseTexture = nullptr;
    std::string m_baseTextureGuid;

    int m_resolution = gotmilked::GameConstants::Terrain::DefaultResolution;
    float m_size = gotmilked::GameConstants::Terrain::DefaultSize;
    float m_minHeight = gotmilked::GameConstants::Terrain::DefaultMinHeight;
    float m_maxHeight = gotmilked::GameConstants::Terrain::DefaultMaxHeight;
    float m_brushRadius = gotmilked::GameConstants::Terrain::DefaultBrushRadius;
    float m_brushStrength = gotmilked::GameConstants::Terrain::DefaultBrushStrength;
    float m_textureTiling = 1.0f;

    bool m_editingEnabled = false;
    bool m_editorWindowVisible = false;
    bool m_meshDirty = false;
    BrushMode m_brushMode = BrushMode::Sculpt;

    std::vector<float> m_heights;
    std::vector<unsigned int> m_indices;

    std::array<PaintLayer, kMaxPaintLayers> m_paintLayers{};
    int m_paintLayerCount = 1;
    int m_activePaintLayer = 0;

    std::function<float()> m_fovProvider;
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
