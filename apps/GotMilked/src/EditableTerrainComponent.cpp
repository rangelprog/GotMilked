#include "EditableTerrainComponent.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/input/InputSystem.hpp"
#include "gm/core/Logger.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float kDefaultFovDegrees = 60.0f;
constexpr float kNearPlane = 0.1f;
constexpr float kFarPlane = 500.0f;
}

EditableTerrainComponent::EditableTerrainComponent() {
    SetName("EditableTerrainComponent");
}

void EditableTerrainComponent::Init() {
    // Only initialize heightmap if it's empty (not loaded from file)
    // This prevents overwriting terrain data loaded from scene files
    if (m_heights.empty()) {
        InitializeHeightmap();
    }
    // Always mark mesh as dirty to ensure it gets rebuilt
    // This is important after loading scenes or when resources are reapplied
    m_meshDirty = true;
}

void EditableTerrainComponent::SetTerrainSize(float sizeMeters) {
    if (sizeMeters <= 0.0f) {
        return;
    }
    m_size = sizeMeters;
    InitializeHeightmap();
    m_meshDirty = true;
}

void EditableTerrainComponent::SetResolution(int resolution) {
    if (resolution < 2) {
        resolution = 2;
    }
    if (resolution == m_resolution) {
        return;
    }
    m_resolution = resolution;
    InitializeHeightmap();
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
            if (leftHeld) {
                ApplyBrush(localXZ, deltaTime, 1.0f);
            }
            if (rightHeld) {
                ApplyBrush(localXZ, deltaTime, -1.0f);
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
            gm::core::Logger::Warning("[EditableTerrain] Failed to rebuild mesh: heights=%zu, resolution=%d, shader=%s",
                m_heights.size(), m_resolution, m_shader ? "set" : "null");
        }
    }

    // Render the mesh if we have all required resources
    if (m_mesh && m_shader) {
        auto owner = GetOwner();
        if (owner) {
            auto transform = owner->EnsureTransform();
            if (transform) {
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
        }
    }

    // Always render the editor window if visible, even if mesh/shader aren't ready
    // This allows the window to be shown for configuration
    if (ImGui::GetCurrentContext() && m_editorWindowVisible) {
        bool windowOpen = m_editorWindowVisible;
        if (ImGui::Begin("Terrain Editor", &windowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Checkbox("Enable Editing", &m_editingEnabled);
            ImGui::Separator();
            ImGui::TextUnformatted("Hold LMB to raise terrain.");
            ImGui::TextUnformatted("Hold RMB to lower terrain.");
            ImGui::Separator();
            if (ImGui::SliderFloat("Brush Radius", &m_brushRadius, 0.5f, 5.0f, "%.2f m")) {
                m_brushRadius = std::clamp(m_brushRadius, 0.1f, 20.0f);
            }
            if (ImGui::SliderFloat("Brush Strength", &m_brushStrength, 0.1f, 5.0f, "%.2f m/s")) {
                m_brushStrength = std::clamp(m_brushStrength, 0.01f, 20.0f);
            }
            ImGui::Separator();
            if (ImGui::SliderFloat("Min Height", &m_minHeight, -10.0f, 0.0f, "%.2f m")) {
                if (m_minHeight > m_maxHeight) {
                    m_minHeight = m_maxHeight;
                }
            }
            if (ImGui::SliderFloat("Max Height", &m_maxHeight, 0.5f, 10.0f, "%.2f m")) {
                if (m_maxHeight < m_minHeight) {
                    m_maxHeight = m_minHeight;
                }
            }
            if (ImGui::SliderFloat("Terrain Size", &m_size, 5.0f, 100.0f, "%.1f m")) {
                m_size = std::max(1.0f, m_size);
                m_meshDirty = true;
            }
            int resolution = m_resolution;
            if (ImGui::InputInt("Resolution", &resolution)) {
                if (resolution != m_resolution && resolution >= 2 && resolution <= 256) {
                    SetResolution(resolution);
                }
            }
        }
        ImGui::End();
        if (!windowOpen) {
            m_editorWindowVisible = false;
            m_editingEnabled = false;
        }
    }
}

void EditableTerrainComponent::InitializeHeightmap() {
    if (m_resolution < 2) {
        m_resolution = 2;
    }

    BuildIndexBuffer();

    const std::size_t vertCount = static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution);
    m_heights.assign(vertCount, 0.0f);
    m_meshDirty = true;
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
        gm::core::Logger::Error("[EditableTerrainComponent] SetHeightData failed: resolution (%d) must be at least 2", resolution);
        return false;
    }
    const std::size_t expected = static_cast<std::size_t>(resolution) * static_cast<std::size_t>(resolution);
    if (heights.size() != expected) {
        gm::core::Logger::Error("[EditableTerrainComponent] SetHeightData failed: expected %zu height values, got %zu", 
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
    vertexData.reserve(static_cast<std::size_t>(m_resolution) * static_cast<std::size_t>(m_resolution) * 8);

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
        }
    }

    gm::Mesh newMesh = gm::Mesh::fromIndexed(vertexData, m_indices);
    if (!m_mesh) {
        m_mesh = std::make_unique<gm::Mesh>();
    }
    *m_mesh = std::move(newMesh);
    return true;
}

void EditableTerrainComponent::ApplyBrush(const glm::vec2& localXZ, float deltaTime, float direction) {
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

    const float fov = m_fovProvider ? m_fovProvider() : kDefaultFovDegrees;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    const glm::mat4 projection = glm::perspective(glm::radians(fov), aspect, kNearPlane, kFarPlane);
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
