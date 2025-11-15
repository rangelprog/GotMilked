#if GM_DEBUG_TOOLS

#include "EditableTerrainComponent.hpp"
#include "GameConstants.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/input/InputSystem.hpp"
#include "gm/core/Logger.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace gm::debug {

EditableTerrainComponent::EditableTerrainComponent() {
    SetName("EditableTerrainComponent");
}

void EditableTerrainComponent::SetActivePaintLayerIndex(int index) {
    if (m_paintLayerCount == 0) {
        m_activePaintLayer = 0;
        return;
    }
    if (index < 0) {
        index = 0;
    }
    if (index >= m_paintLayerCount) {
        index = m_paintLayerCount - 1;
    }
    m_activePaintLayer = index;
    if (m_paintLayerCount > 0) {
        EnsureLayerWeightsSize(m_paintLayers[m_activePaintLayer].weights);
    }
}

bool EditableTerrainComponent::AddPaintLayer() {
    if (!CanAddPaintLayer()) {
        return false;
    }
    PaintLayer& layer = m_paintLayers[m_paintLayerCount];
    layer.guid.clear();
    layer.texture = nullptr;
    EnsureLayerWeightsSize(layer.weights);
    layer.hasPaint = false;
    layer.enabled = true;
    ++m_paintLayerCount;
    UpdatePaintLayerState();
    m_activePaintLayer = m_paintLayerCount - 1;
    m_meshDirty = true;
    return true;
}

void EditableTerrainComponent::SetPaintLayerCount(int count) {
    int clamped = std::clamp(count, 1, kMaxPaintLayers);
    if (clamped == m_paintLayerCount) {
        return;
    }

    if (clamped < m_paintLayerCount) {
        for (int i = clamped; i < m_paintLayerCount; ++i) {
            m_paintLayers[i] = PaintLayer{};
        }
    }

    m_paintLayerCount = clamped;
    if (m_activePaintLayer >= m_paintLayerCount) {
        m_activePaintLayer = m_paintLayerCount - 1;
    }

    const std::size_t expected = static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution);
    for (int i = 0; i < m_paintLayerCount; ++i) {
        auto& weights = m_paintLayers[i].weights;
        if (weights.size() != expected) {
            weights.assign(expected, 0.0f);
        }
    }

    UpdatePaintLayerState();
    m_meshDirty = true;
}

const std::string& EditableTerrainComponent::GetPaintTextureGuid() const {
    return GetPaintTextureGuid(m_activePaintLayer);
}

const std::string& EditableTerrainComponent::GetPaintTextureGuid(int layer) const {
    static const std::string kEmptyGuid;
    if (layer < 0 || layer >= m_paintLayerCount) {
        return kEmptyGuid;
    }
    return m_paintLayers[layer].guid;
}

gm::Texture* EditableTerrainComponent::GetPaintTexture() const {
    return GetPaintTexture(m_activePaintLayer);
}

gm::Texture* EditableTerrainComponent::GetPaintTexture(int layer) const {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return nullptr;
    }
    return m_paintLayers[layer].texture;
}

bool EditableTerrainComponent::PaintLayerHasTexture(int layer) const {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return false;
    }
    return m_paintLayers[layer].texture != nullptr;
}

bool EditableTerrainComponent::PaintLayerHasPaint(int layer) const {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return false;
    }
    return m_paintLayers[layer].hasPaint;
}

const std::vector<float>& EditableTerrainComponent::GetPaintLayerWeights(int layer) const {
    static const std::vector<float> kEmptyWeights;
    if (layer < 0 || layer >= m_paintLayerCount) {
        return kEmptyWeights;
    }
    return m_paintLayers[layer].weights;
}

bool EditableTerrainComponent::IsPaintLayerEnabled(int layer) const {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return false;
    }
    return m_paintLayers[layer].enabled;
}

void EditableTerrainComponent::SetPaintLayerEnabled(int layer, bool enabled) {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return;
    }
    m_paintLayers[layer].enabled = enabled;
    UpdatePaintLayerState();
    m_meshDirty = true;
}

bool EditableTerrainComponent::RemovePaintLayer(int layer) {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return false;
    }
    if (m_paintLayerCount <= 1) {
        return false;
    }
    for (int i = layer; i < m_paintLayerCount - 1; ++i) {
        m_paintLayers[i] = m_paintLayers[i + 1];
    }
    auto& clearedLayer = m_paintLayers[m_paintLayerCount - 1];
    clearedLayer = PaintLayer{};
    clearedLayer.weights.clear();
    --m_paintLayerCount;
    if (m_activePaintLayer >= m_paintLayerCount) {
        m_activePaintLayer = std::max(0, m_paintLayerCount - 1);
    }
    UpdatePaintLayerState();
    EnsureLayerWeightsSize(m_paintLayers[m_activePaintLayer].weights);
    m_meshDirty = true;
    return true;
}

void EditableTerrainComponent::BindPaintTexture(int layer, gm::Texture* texture) {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return;
    }
    m_paintLayers[layer].texture = texture;
    m_meshDirty = true;
}

void EditableTerrainComponent::SetPaintLayerData(int layer, const std::string& guid, bool enabled, const std::vector<float>& weights) {
    if (layer < 0 || layer >= m_paintLayerCount) {
        return;
    }

    PaintLayer& target = m_paintLayers[layer];
    target.guid = guid;
    target.texture = nullptr;
    target.enabled = enabled;

    const std::size_t expected = static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution);
    target.weights.assign(expected, 0.0f);
    if (!weights.empty()) {
        const std::size_t count = std::min(expected, weights.size());
        std::copy_n(weights.begin(), count, target.weights.begin());
    }

    target.hasPaint = std::any_of(target.weights.begin(), target.weights.end(), [](float value) {
        return value > 1e-3f;
    });

    UpdatePaintLayerState();
    m_meshDirty = true;
}

void EditableTerrainComponent::Init() {
    // Only initialize heightmap if it's empty (not loaded from file)
    // This prevents overwriting terrain data loaded from scene files
    if (m_heights.empty()) {
        InitializeHeightmap();
    } else {
        for (int i = 0; i < m_paintLayerCount; ++i) {
            EnsureLayerWeightsSize(m_paintLayers[i].weights);
        }
        UpdatePaintLayerState();
    }
    // Always mark mesh as dirty to ensure it gets rebuilt
    // This is important after loading scenes or when resources are reapplied
    m_meshDirty = true;

    if (m_material && m_baseTexture) {
        m_material->SetDiffuseTexture(m_baseTexture);
    }
}

void EditableTerrainComponent::SetTerrainSize(float sizeMeters) {
    if (sizeMeters <= 0.0f) {
        return;
    }

    if (std::abs(sizeMeters - m_size) < 1e-4f) {
        return;
    }

    m_size = sizeMeters;

    if (m_heights.empty()) {
        InitializeHeightmap();
    }

    m_meshDirty = true;
}

void EditableTerrainComponent::SetMinHeight(float height) {
    m_minHeight = height;
    if (m_minHeight > m_maxHeight) {
        m_minHeight = m_maxHeight;
    }
}

void EditableTerrainComponent::SetMaxHeight(float height) {
    m_maxHeight = height;
    if (m_maxHeight < m_minHeight) {
        m_maxHeight = m_minHeight;
    }
}

void EditableTerrainComponent::SetBrushRadius(float radius) {
    m_brushRadius = std::clamp(radius,
        gotmilked::GameConstants::Terrain::MinBrushRadius,
        gotmilked::GameConstants::Terrain::MaxBrushRadius);
}

void EditableTerrainComponent::SetBrushStrength(float strength) {
    m_brushStrength = std::clamp(strength,
        gotmilked::GameConstants::Terrain::MinBrushStrength,
        gotmilked::GameConstants::Terrain::MaxBrushStrength);
}

void EditableTerrainComponent::SetResolution(int resolution) {
    if (resolution < 2) {
        resolution = 2;
    }
    if (resolution == m_resolution) {
        return;
    }

    if (m_heights.empty() || m_resolution < 2) {
        m_resolution = resolution;
        InitializeHeightmap();
    } else {
        const int previousResolution = m_resolution;
        std::array<std::vector<float>, kMaxPaintLayers> previousPaintWeights;
        for (int i = 0; i < kMaxPaintLayers; ++i) {
            previousPaintWeights[i] = m_paintLayers[i].weights;
        }
        ResampleHeightmap(resolution);
        ResamplePaintLayers(previousResolution, resolution, previousPaintWeights);
    }
    m_meshDirty = true;
}

void EditableTerrainComponent::Update(float deltaTime) {
    auto* inputSystem = gm::core::Input::Instance().GetInputSystem();
    if (!inputSystem || !m_camera || !m_window) {
        return;
    }

    if (inputSystem->IsKeyJustPressed(GLFW_KEY_T)) {
        m_editingEnabled = !m_editingEnabled;
    }

    bool leftHeld = inputSystem->IsMouseButtonHeld(gm::core::MouseButton::Left);
    bool rightHeld = inputSystem->IsMouseButtonHeld(gm::core::MouseButton::Right);

    bool wantsMouse = false;
    if (ImGui::GetCurrentContext()) {
        wantsMouse = ImGui::GetIO().WantCaptureMouse;
    }

    if (m_editingEnabled && (leftHeld || rightHeld) && !wantsMouse) {
        glm::vec3 worldHit;
        glm::vec2 localXZ;
        if (ComputeTerrainHit(worldHit, localXZ)) {
            if (m_brushMode == BrushMode::Sculpt) {
                if (leftHeld) {
                    ApplyHeightBrush(localXZ, deltaTime, 1.0f);
                }
                if (rightHeld) {
                    ApplyHeightBrush(localXZ, deltaTime, -1.0f);
                }
            } else if (m_brushMode == BrushMode::Paint && PaintLayerHasTexture(m_activePaintLayer)) {
                if (leftHeld) {
                    ApplyTextureBrush(localXZ, deltaTime, 1.0f);
                }
                if (rightHeld) {
                    ApplyTextureBrush(localXZ, deltaTime, -1.0f);
                }
            }
        }
    }
}

void EditableTerrainComponent::Render() {
    // Rebuild mesh if dirty (this will create the mesh if it doesn't exist)
    // Note: RebuildMesh doesn't need shader, only rendering does
    if (m_meshDirty) {
        if (RebuildMesh()) {
            m_meshDirty = false;
            gm::core::Logger::Debug("[EditableTerrain] Mesh rebuilt successfully");
        } else {
            gm::core::Logger::Warning("[EditableTerrain] Failed to rebuild mesh: heights={}, resolution={}, shader={}",
                m_heights.size(), m_resolution, m_shader ? "set" : "null");
        }
    }

    // Render the mesh if we have all required resources
    if (m_mesh && m_shader) {
        auto* gameObject = GetOwner();
        if (!gameObject) {
            return;
        }
        auto* scene = gameObject->GetScene();
        if (!scene || !scene->HasRenderContext()) {
            return;
        }
        auto transform = gameObject->EnsureTransform();
        if (transform) {
            const glm::mat4 model = transform->GetMatrix();
            const glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));
            const glm::mat4& view = scene->CurrentViewMatrix();
            const glm::mat4& proj = scene->CurrentProjectionMatrix();
            const glm::vec3& camPos = scene->CurrentCameraPosition();

            m_shader->Use();
            m_shader->SetInt("uUseInstanceBuffers", 0);
            m_shader->SetMat4("uModel", model);
            m_shader->SetMat3("uNormalMat", normalMat);
            m_shader->SetMat4("uView", view);
            m_shader->SetMat4("uProj", proj);
            m_shader->SetVec3("uViewPos", camPos);

            if (m_material) {
                if (m_baseTexture && m_material->GetDiffuseTexture() != m_baseTexture) {
                    m_material->SetDiffuseTexture(m_baseTexture);
                } else if (!m_baseTexture && m_material->GetDiffuseTexture() != nullptr) {
                    m_material->SetDiffuseTexture(nullptr);
                }
                m_material->Apply(*m_shader);
            }

            if (m_baseTexture) {
                m_shader->SetInt("uUseTex", 1);
                m_baseTexture->bind(0);
                m_shader->SetInt("uTex", 0);
            } else {
                m_shader->SetInt("uUseTex", 0);
            }

            m_shader->SetFloat("uTextureTiling", m_textureTiling);

            m_shader->SetInt("uPaintLayerCount", m_paintLayerCount);
            bool anyPaint = false;
            for (int i = 0; i < kMaxPaintLayers; ++i) {
                std::string enabledUniform = "uPaintLayerEnabled[" + std::to_string(i) + "]";
                if (i < m_paintLayerCount && m_paintLayers[i].texture && m_paintLayers[i].enabled) {
                    const int textureSlot = 8 + i;
                    m_paintLayers[i].texture->bind(textureSlot);
                    m_shader->SetInt(("uPaintLayers[" + std::to_string(i) + "]").c_str(), textureSlot);
                    m_shader->SetInt(enabledUniform.c_str(), 1);
                    if (m_paintLayers[i].hasPaint) {
                        anyPaint = true;
                    }
                } else {
                    m_shader->SetInt(enabledUniform.c_str(), 0);
                }
            }
            m_shader->SetInt("uUsePaint", anyPaint ? 1 : 0);

            m_mesh->Draw();
        }
    }

    // Terrain editor UI is now in the Inspector window
}

void EditableTerrainComponent::InitializeHeightmap() {
    if (m_resolution < 2) {
        m_resolution = 2;
    }

    if (m_paintLayerCount <= 0) {
        m_paintLayerCount = 1;
        m_activePaintLayer = 0;
    }

    BuildIndexBuffer();

    const std::size_t vertCount = static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution);
    m_heights.assign(vertCount, 0.0f);
    for (int i = 0; i < kMaxPaintLayers; ++i) {
        m_paintLayers[i].weights.clear();
        m_paintLayers[i].hasPaint = false;
        if (i < m_paintLayerCount) {
            m_paintLayers[i].weights.assign(vertCount, 0.0f);
        }
    }
    UpdatePaintLayerState();
    m_meshDirty = true;
}

void EditableTerrainComponent::ResampleHeightmap(int newResolution) {
    if (newResolution < 2) {
        newResolution = 2;
    }

    if (m_heights.empty() || m_resolution < 2) {
        m_resolution = newResolution;
        InitializeHeightmap();
        return;
    }

    const int oldResolution = m_resolution;
    const std::vector<float> oldHeights = m_heights;

    auto sampleOld = [&](float u, float v) {
        const float x = u * static_cast<float>(oldResolution - 1);
        const float z = v * static_cast<float>(oldResolution - 1);
        const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, oldResolution - 1);
        const int z0 = std::clamp(static_cast<int>(std::floor(z)), 0, oldResolution - 1);
        const int x1 = std::min(x0 + 1, oldResolution - 1);
        const int z1 = std::min(z0 + 1, oldResolution - 1);
        const float tx = x - static_cast<float>(x0);
        const float tz = z - static_cast<float>(z0);
        const auto index = [&](int ix, int iz) {
            return static_cast<std::size_t>(iz) * static_cast<std::size_t>(oldResolution) + static_cast<std::size_t>(ix);
        };
        const float h00 = oldHeights[index(x0, z0)];
        const float h10 = oldHeights[index(x1, z0)];
        const float h01 = oldHeights[index(x0, z1)];
        const float h11 = oldHeights[index(x1, z1)];
        const float hx0 = h00 + (h10 - h00) * tx;
        const float hx1 = h01 + (h11 - h01) * tx;
        const float h = hx0 + (hx1 - hx0) * tz;
        return std::clamp(h, m_minHeight, m_maxHeight);
    };

    m_resolution = newResolution;
    const std::size_t newCount = static_cast<std::size_t>(newResolution) * static_cast<std::size_t>(newResolution);
    m_heights.assign(newCount, 0.0f);

    for (int z = 0; z < newResolution; ++z) {
        const float v = (newResolution > 1) ? static_cast<float>(z) / static_cast<float>(newResolution - 1) : 0.0f;
        for (int x = 0; x < newResolution; ++x) {
            const float u = (newResolution > 1) ? static_cast<float>(x) / static_cast<float>(newResolution - 1) : 0.0f;
            const std::size_t idx = static_cast<std::size_t>(z) * static_cast<std::size_t>(newResolution) + static_cast<std::size_t>(x);
            m_heights[idx] = sampleOld(u, v);
        }
    }

    BuildIndexBuffer();
}

void EditableTerrainComponent::ResamplePaintLayers(int oldResolution, int newResolution,
        const std::array<std::vector<float>, kMaxPaintLayers>& oldWeights) {
    if (newResolution < 2) {
        newResolution = 2;
    }

    const std::size_t newCount = static_cast<std::size_t>(newResolution) * static_cast<std::size_t>(newResolution);

    for (int layerIndex = 0; layerIndex < m_paintLayerCount; ++layerIndex) {
        const auto& oldBlend = oldWeights[layerIndex];
        PaintLayer& layer = m_paintLayers[layerIndex];
        layer.weights.assign(newCount, 0.0f);

        if (oldBlend.empty() || oldResolution < 2) {
            layer.hasPaint = false;
            continue;
        }

        auto sampleOld = [&](float u, float v) {
            const float x = u * static_cast<float>(oldResolution - 1);
            const float z = v * static_cast<float>(oldResolution - 1);
            const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, oldResolution - 1);
            const int z0 = std::clamp(static_cast<int>(std::floor(z)), 0, oldResolution - 1);
            const int x1 = std::min(x0 + 1, oldResolution - 1);
            const int z1 = std::min(z0 + 1, oldResolution - 1);
            const float tx = x - static_cast<float>(x0);
            const float tz = z - static_cast<float>(z0);
            const auto index = [&](int ix, int iz) {
                return static_cast<std::size_t>(iz) * static_cast<std::size_t>(oldResolution) + static_cast<std::size_t>(ix);
            };
            const float b00 = oldBlend[index(x0, z0)];
            const float b10 = oldBlend[index(x1, z0)];
            const float b01 = oldBlend[index(x0, z1)];
            const float b11 = oldBlend[index(x1, z1)];
            const float bx0 = b00 + (b10 - b00) * tx;
            const float bx1 = b01 + (b11 - b01) * tx;
            return std::clamp(bx0 + (bx1 - bx0) * tz, 0.0f, 1.0f);
        };

        for (int z = 0; z < newResolution; ++z) {
            const float v = (newResolution > 1) ? static_cast<float>(z) / static_cast<float>(newResolution - 1) : 0.0f;
            for (int x = 0; x < newResolution; ++x) {
                const float u = (newResolution > 1) ? static_cast<float>(x) / static_cast<float>(newResolution - 1) : 0.0f;
                const std::size_t idx = static_cast<std::size_t>(z) * static_cast<std::size_t>(newResolution) + static_cast<std::size_t>(x);
                layer.weights[idx] = sampleOld(u, v);
            }
        }
    }

    for (int i = m_paintLayerCount; i < kMaxPaintLayers; ++i) {
        m_paintLayers[i].weights.clear();
        m_paintLayers[i].hasPaint = false;
    }

    UpdatePaintLayerState();
}

void EditableTerrainComponent::BuildIndexBuffer() {
    m_indices.clear();
    if (m_resolution < 2) {
        return;
    }

    m_indices.reserve(static_cast<std::size_t>(m_resolution - 1) * static_cast<std::size_t>(m_resolution - 1) * 6);
    for (int z = 0; z < m_resolution - 1; ++z) {
        for (int x = 0; x < m_resolution - 1; ++x) {
            const int topLeft = z * m_resolution + x;
            const int topRight = topLeft + 1;
            const int bottomLeft = (z + 1) * m_resolution + x;
            const int bottomRight = bottomLeft + 1;

            m_indices.push_back(topLeft);
            m_indices.push_back(bottomLeft);
            m_indices.push_back(topRight);

            m_indices.push_back(topRight);
            m_indices.push_back(bottomLeft);
            m_indices.push_back(bottomRight);
        }
    }
}

bool EditableTerrainComponent::SetHeightData(int resolution,
                                              float size,
                                              float minHeight,
                                              float maxHeight,
                                              const std::vector<float>& heights) {
    if (resolution < 2) {
        gm::core::Logger::Error("[EditableTerrainComponent] SetHeightData failed: resolution ({}) must be at least 2", resolution);
        return false;
    }
    const std::size_t expected = static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
    if (heights.size() != expected) {
        gm::core::Logger::Error("[EditableTerrainComponent] SetHeightData failed: expected {} height values, got {}", 
                    expected, heights.size());
        return false;
    }

    m_resolution = resolution;
    m_size = size;
    m_minHeight = minHeight;
    m_maxHeight = maxHeight;

    m_heights.resize(expected);
    for (std::size_t i = 0; i < expected; ++i) {
        float value = heights[i];
        if (!std::isfinite(value)) {
            value = 0.0f;
        }
        m_heights[i] = std::clamp(value, m_minHeight, m_maxHeight);
    }

    BuildIndexBuffer();
    m_meshDirty = true;
    for (int i = 0; i < kMaxPaintLayers; ++i) {
        m_paintLayers[i].weights.clear();
        m_paintLayers[i].hasPaint = false;
        if (i < m_paintLayerCount) {
            m_paintLayers[i].weights.assign(expected, 0.0f);
        }
    }
    return true;
}

bool EditableTerrainComponent::RebuildMesh() {
    if (m_heights.empty() || m_resolution < 2) {
        return false;
    }

    const float halfSize = m_size * 0.5f;
    const float spacing = m_size / static_cast<float>(m_resolution - 1);

    if (spacing <= 0.0f || !std::isfinite(spacing)) {
        return false;
    }

    std::vector<float> vertexData;
    vertexData.reserve(static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution) * 12);

    const std::size_t expectedSize = static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution);
    if (m_heights.size() != expectedSize) {
        return false;
    }

    auto sampleHeight = [&](int x, int z) {
        x = std::clamp(x, 0, m_resolution - 1);
        z = std::clamp(z, 0, m_resolution - 1);
        const std::size_t idx = static_cast<std::size_t>(z) * static_cast<std::size_t>(m_resolution) + static_cast<std::size_t>(x);
        if (idx >= m_heights.size()) {
            return 0.0f;
        }
        return m_heights[idx];
    };

    for (int z = 0; z < m_resolution; ++z) {
        for (int x = 0; x < m_resolution; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(m_resolution - 1);
            const float v = static_cast<float>(z) / static_cast<float>(m_resolution - 1);
            const std::size_t idx = static_cast<std::size_t>(z) * static_cast<std::size_t>(m_resolution) + static_cast<std::size_t>(x);
            const float height = (idx < m_heights.size()) ? m_heights[idx] : 0.0f;
            std::array<float, kMaxPaintLayers> paintWeights{};
            paintWeights.fill(0.0f);
            for (int layerIndex = 0; layerIndex < m_paintLayerCount; ++layerIndex) {
                const auto& weights = m_paintLayers[layerIndex].weights;
                if (idx < weights.size()) {
                    paintWeights[layerIndex] = weights[idx];
                }
            }

            const float px = -halfSize + u * m_size;
            const float pz = -halfSize + v * m_size;

            const float hL = sampleHeight(x - 1, z);
            const float hR = sampleHeight(x + 1, z);
            const float hD = sampleHeight(x, z - 1);
            const float hU = sampleHeight(x, z + 1);
            glm::vec3 normal(hL - hR, 2.0f * spacing, hD - hU);
            const float normalLenSq = glm::dot(normal, normal);
            if (normalLenSq < 1e-6f) {
                normal = glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                normal = glm::normalize(normal);
            }

            vertexData.push_back(px);
            vertexData.push_back(height);
            vertexData.push_back(pz);
            vertexData.push_back(normal.x);
            vertexData.push_back(normal.y);
            vertexData.push_back(normal.z);
            vertexData.push_back(u);
            vertexData.push_back(v);
            vertexData.push_back(paintWeights[0]);
            vertexData.push_back(paintWeights[1]);
            vertexData.push_back(paintWeights[2]);
            vertexData.push_back(paintWeights[3]);
        }
    }

    gm::Mesh newMesh = gm::Mesh::fromIndexed(vertexData, m_indices, 12);
    if (!m_mesh) {
        m_mesh = std::make_unique<gm::Mesh>();
    }
    *m_mesh = std::move(newMesh);
    return true;
}

void EditableTerrainComponent::ApplyHeightBrush(const glm::vec2& localXZ, float deltaTime, float direction) {
    if (m_resolution < 2) {
        return;
    }

    const float halfSize = m_size * 0.5f;
    const float spacing = m_size / static_cast<float>(m_resolution - 1);

    if (!std::isfinite(localXZ.x) || !std::isfinite(localXZ.y) || !std::isfinite(deltaTime) || !std::isfinite(direction)) {
        return;
    }

    if (m_brushRadius <= 0.0f || !std::isfinite(m_brushRadius)) {
        return;
    }

    const std::size_t expectedSize = static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution);
    if (m_heights.size() != expectedSize || expectedSize == 0) {
        return;
    }

    const float localX = localXZ.x + halfSize;
    const float localZ = localXZ.y + halfSize;

    if (localX < 0.0f || localX > m_size || localZ < 0.0f || localZ > m_size) {
        return;
    }

    if (spacing <= 0.0f || !std::isfinite(spacing)) {
        return;
    }

    const float targetX = localX / spacing;
    const float targetZ = localZ / spacing;

    const int minX = std::max(0, static_cast<int>(std::floor((localX - m_brushRadius) / spacing)));
    const int maxX = std::min(m_resolution - 1, static_cast<int>(std::ceil((localX + m_brushRadius) / spacing)));
    const int minZ = std::max(0, static_cast<int>(std::floor((localZ - m_brushRadius) / spacing)));
    const int maxZ = std::min(m_resolution - 1, static_cast<int>(std::ceil((localZ + m_brushRadius) / spacing)));

    if (minX > maxX || minZ > maxZ) {
        return;
    }

    bool modified = false;
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            if (x < 0 || x >= m_resolution || z < 0 || z >= m_resolution) {
                continue;
            }

            const float dx = (static_cast<float>(x) - targetX) * spacing;
            const float dz = (static_cast<float>(z) - targetZ) * spacing;
            const float distSq = dx * dx + dz * dz;
            if (distSq > m_brushRadius * m_brushRadius) {
                continue;
            }
            const float dist = std::sqrt(distSq);
            if (dist > m_brushRadius) {
                continue;
            }
            const float falloff = 1.0f - (dist / m_brushRadius);
            const float delta = direction * m_brushStrength * falloff * deltaTime;
            const std::size_t idx = static_cast<std::size_t>(z) * static_cast<std::size_t>(m_resolution) + static_cast<std::size_t>(x);
            
            if (idx >= m_heights.size()) {
                continue;
            }

            const float oldHeight = m_heights[idx];
            if (!std::isfinite(oldHeight)) {
                continue;
            }

            const float newHeight = std::clamp(oldHeight + delta, m_minHeight, m_maxHeight);
            if (!std::isfinite(newHeight)) {
                continue;
            }
            if (std::abs(newHeight - oldHeight) > 1e-5f) {
                m_heights[idx] = newHeight;
                modified = true;
            }
        }
    }

    if (modified) {
        m_meshDirty = true;
    }
}

void EditableTerrainComponent::ApplyTextureBrush(const glm::vec2& localXZ, float deltaTime, float direction) {
    if (m_paintLayerCount == 0) {
        return;
    }

    PaintLayer& layer = m_paintLayers[m_activePaintLayer];
    if (!layer.texture || !layer.enabled) {
        return;
    }
    EnsureLayerWeightsSize(layer.weights);

    if (m_resolution < 2 || layer.weights.size() != static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution)) {
        return;
    }

    const float halfSize = m_size * 0.5f;
    const float spacing = m_size / static_cast<float>(m_resolution - 1);

    if (!std::isfinite(localXZ.x) || !std::isfinite(localXZ.y) || !std::isfinite(deltaTime) || !std::isfinite(direction)) {
        return;
    }
    if (m_brushRadius <= 0.0f || !std::isfinite(m_brushRadius)) {
        return;
    }
    if (spacing <= 0.0f || !std::isfinite(spacing)) {
        return;
    }

    const float localX = localXZ.x + halfSize;
    const float localZ = localXZ.y + halfSize;
    if (localX < 0.0f || localX > m_size || localZ < 0.0f || localZ > m_size) {
        return;
    }

    const float targetX = localX / spacing;
    const float targetZ = localZ / spacing;

    const int minX = std::max(0, static_cast<int>(std::floor((localX - m_brushRadius) / spacing)));
    const int maxX = std::min(m_resolution - 1, static_cast<int>(std::ceil((localX + m_brushRadius) / spacing)));
    const int minZ = std::max(0, static_cast<int>(std::floor((localZ - m_brushRadius) / spacing)));
    const int maxZ = std::min(m_resolution - 1, static_cast<int>(std::ceil((localZ + m_brushRadius) / spacing)));

    if (minX > maxX || minZ > maxZ) {
        return;
    }

    const float intensity = m_brushStrength * deltaTime * direction;
    if (!std::isfinite(intensity) || intensity == 0.0f) {
        return;
    }

    bool modified = false;
    for (int z = minZ; z <= maxZ; ++z) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = (static_cast<float>(x) - targetX) * spacing;
            const float dz = (static_cast<float>(z) - targetZ) * spacing;
            const float distSq = dx * dx + dz * dz;
            if (distSq > m_brushRadius * m_brushRadius) {
                continue;
            }
            const float dist = std::sqrt(distSq);
            const float falloff = 1.0f - (dist / m_brushRadius);
            const float delta = intensity * falloff;

            const std::size_t idx = static_cast<std::size_t>(z) * static_cast<std::size_t>(m_resolution) + static_cast<std::size_t>(x);
            float weight = layer.weights[idx];
            float sumOthers = 0.0f;
            for (int other = 0; other < m_paintLayerCount; ++other) {
                if (other == m_activePaintLayer) {
                    continue;
                }
                const auto& otherWeights = m_paintLayers[other].weights;
                if (idx < otherWeights.size()) {
                    sumOthers += std::clamp(otherWeights[idx], 0.0f, 1.0f);
                }
            }
            sumOthers = std::clamp(sumOthers, 0.0f, 1.0f);
            float maxAvailable = std::max(0.0f, 1.0f - sumOthers);
            float newWeight = glm::clamp(weight + delta, 0.0f, maxAvailable);
            if (std::abs(newWeight - weight) > 1e-4f) {
                layer.weights[idx] = newWeight;
                modified = true;
            }
        }
    }

    if (modified) {
        layer.hasPaint = std::any_of(layer.weights.begin(), layer.weights.end(), [](float value) {
            return value > 1e-3f;
        });
        UpdatePaintLayerState();
        m_meshDirty = true;
    }
}

bool EditableTerrainComponent::ComputeTerrainHit(glm::vec3& outWorldPos, glm::vec2& outLocalXZ) const {
    if (!m_camera || !m_window) {
        return false;
    }

    int width = 0;
    int height = 0;
    glfwGetWindowSize(m_window, &width, &height);
    if (width <= 0 || height <= 0) {
        return false;
    }

    const glm::vec2 mousePos = gm::core::Input::Instance().GetMousePosition();
    const float ndcX = (2.0f * mousePos.x) / static_cast<float>(width) - 1.0f;
    const float ndcY = 1.0f - (2.0f * mousePos.y) / static_cast<float>(height);

    const float fov = m_fovProvider ? m_fovProvider() : gotmilked::GameConstants::Camera::DefaultFovDegrees;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    const glm::mat4 projection = glm::perspective(
        glm::radians(fov), 
        aspect, 
        gotmilked::GameConstants::Camera::NearPlane, 
        gotmilked::GameConstants::Camera::FarPlane);
    const glm::mat4 view = m_camera->View();
    const glm::mat4 invProj = glm::inverse(projection);
    const glm::mat4 invView = glm::inverse(view);

    glm::vec4 clipRay(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 eyeRay = invProj * clipRay;
    eyeRay = glm::vec4(eyeRay.x, eyeRay.y, -1.0f, 0.0f);
    glm::vec3 worldDir = glm::normalize(glm::vec3(invView * eyeRay));

    if (!std::isfinite(worldDir.x) || !std::isfinite(worldDir.y) || !std::isfinite(worldDir.z)) {
        return false;
    }

    const glm::vec3 origin = m_camera->Position();
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z)) {
        return false;
    }

    auto transform = GetOwner() ? GetOwner()->GetTransform() : nullptr;
    if (!transform) {
        return false;
    }

    const glm::vec3 planeNormal(0.0f, 1.0f, 0.0f);
    const float planeY = transform->GetPosition().y;
    const float denom = glm::dot(worldDir, planeNormal);
    if (std::abs(denom) < 1e-4f) {
        return false;
    }

    const float t = (planeY - origin.y) / worldDir.y;
    if (t < 0.0f) {
        return false;
    }

    outWorldPos = origin + worldDir * t;

    glm::vec3 local = outWorldPos - transform->GetPosition();
    const glm::vec3 scale = transform->GetScale();
    if (scale.x != 0.0f) local.x /= scale.x;
    if (scale.y != 0.0f) local.y /= scale.y;
    if (scale.z != 0.0f) local.z /= scale.z;

    const float halfSize = m_size * 0.5f;
    if (local.x < -halfSize || local.x > halfSize || local.z < -halfSize || local.z > halfSize) {
        return false;
    }

    outLocalXZ = glm::vec2(local.x, local.z);
    return true;
}

void EditableTerrainComponent::SetTextureTiling(float tiling) {
    m_textureTiling = std::clamp(tiling, 0.1f, 64.0f);
}

void EditableTerrainComponent::SetBaseTexture(const std::string& guid, gm::Texture* texture) {
    m_baseTextureGuid = guid;
    m_baseTexture = texture;
    if (m_material) {
        m_material->SetDiffuseTexture(texture);
    }
    if (texture) {
        ClearAllPaintWeights();
    }
    UpdatePaintLayerState();
    m_meshDirty = true;
}

void EditableTerrainComponent::ClearBaseTexture() {
    m_baseTextureGuid.clear();
    m_baseTexture = nullptr;
    if (m_material) {
        m_material->SetDiffuseTexture(nullptr);
    }
    UpdatePaintLayerState();
    m_meshDirty = true;
}

void EditableTerrainComponent::SetBaseTextureGuidFromSave(const std::string& guid) {
    m_baseTextureGuid = guid;
    if (m_material) {
        m_material->SetDiffuseTexture(nullptr);
    }
    m_baseTexture = nullptr;
    m_meshDirty = true;
}

void EditableTerrainComponent::BindBaseTexture(gm::Texture* texture) {
    m_baseTexture = texture;
    if (m_material) {
        m_material->SetDiffuseTexture(texture);
    }
    m_meshDirty = true;
}

void EditableTerrainComponent::SetPaintTexture(const std::string& guid, gm::Texture* texture) {
    if (m_paintLayerCount == 0) {
        return;
    }
    PaintLayer& layer = m_paintLayers[m_activePaintLayer];
    layer.guid = guid;
    layer.texture = texture;
    EnsureLayerWeightsSize(layer.weights);
    UpdatePaintLayerState();
    m_meshDirty = true;
}

void EditableTerrainComponent::ClearPaintTexture() {
    if (m_paintLayerCount == 0) {
        return;
    }
    PaintLayer& layer = m_paintLayers[m_activePaintLayer];
    layer.guid.clear();
    layer.texture = nullptr;
    if (!layer.weights.empty()) {
        std::fill(layer.weights.begin(), layer.weights.end(), 0.0f);
    }
    layer.hasPaint = false;
    UpdatePaintLayerState();
    m_meshDirty = true;
}

void EditableTerrainComponent::FillPaintLayer(float weight) {
    if (m_paintLayerCount == 0) {
        return;
    }
    PaintLayer& layer = m_paintLayers[m_activePaintLayer];
    if (!layer.texture || !layer.enabled) {
        return;
    }
    const float clamped = std::clamp(weight, 0.0f, 1.0f);
    EnsureLayerWeightsSize(layer.weights);
    std::fill(layer.weights.begin(), layer.weights.end(), clamped);
    layer.hasPaint = clamped > 1e-3f;
    UpdatePaintLayerState();
    m_meshDirty = true;
}

void EditableTerrainComponent::UpdatePaintLayerState() {
    for (int i = 0; i < m_paintLayerCount; ++i) {
        auto& layer = m_paintLayers[i];
        if (layer.weights.empty() || !layer.enabled) {
            layer.hasPaint = false;
            continue;
        }
        layer.hasPaint = std::any_of(layer.weights.begin(), layer.weights.end(), [](float value) {
            return value > 1e-3f;
        });
    }
    for (int i = m_paintLayerCount; i < kMaxPaintLayers; ++i) {
        m_paintLayers[i].hasPaint = false;
    }
}

void EditableTerrainComponent::EnsureLayerWeightsSize(std::vector<float>& weights) {
    const std::size_t expected = static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution);
    if (weights.size() != expected) {
        weights.assign(expected, 0.0f);
    }
}

void EditableTerrainComponent::ClearAllPaintWeights() {
    for (int i = 0; i < m_paintLayerCount; ++i) {
        auto& layer = m_paintLayers[i];
        EnsureLayerWeightsSize(layer.weights);
        std::fill(layer.weights.begin(), layer.weights.end(), 0.0f);
        layer.hasPaint = false;
    }
    UpdatePaintLayerState();
    m_meshDirty = true;
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
