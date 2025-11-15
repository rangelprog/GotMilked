#if GM_DEBUG_TOOLS

#include "../DebugMenu.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/VolumetricFogComponent.hpp"

#include <fmt/format.h>

#include <algorithm>

namespace gm::debug {

namespace {

struct FogRow {
    std::shared_ptr<gm::GameObject> object;
    std::shared_ptr<gm::VolumetricFogComponent> component;
    float radius = 1.0f;
};

} // namespace

void DebugMenu::RenderFogDebugger() {
    if (!ImGui::Begin("Volumetric Fog Debugger", &m_showFogDebugger)) {
        ImGui::End();
        return;
    }

    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextUnformatted("No active scene.");
        ImGui::End();
        return;
    }

    std::vector<FogRow> rows;
    rows.reserve(scene->GetAllGameObjects().size());

    int activeCount = 0;
    int disabledCount = 0;

    for (const auto& object : scene->GetAllGameObjects()) {
        if (!object) {
            continue;
        }

        auto fog = object->GetComponent<gm::VolumetricFogComponent>();
        if (!fog) {
            continue;
        }

        FogRow row;
        row.object = object;
        row.component = fog;
        if (auto transform = object->GetTransform()) {
            const glm::vec3 scale = transform->GetScale();
            row.radius = std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z), 0.25f});
        }
        if (fog->IsEnabled()) {
            ++activeCount;
        } else {
            ++disabledCount;
        }
        rows.emplace_back(std::move(row));
    }

    ImGui::Text("Fog volumes: %d active / %d disabled", activeCount, disabledCount);
    ImGui::SameLine();
    ImGui::Text("Overlay: %s", m_fogDebug.overlayEnabled ? "ON" : "OFF");

    if (ImGui::CollapsingHeader("Overlay Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Draw overlay gizmos", &m_fogDebug.overlayEnabled)) {
            // nothing else to do
        }
        ImGui::BeginDisabled(!m_fogDebug.overlayEnabled);
        ImGui::Checkbox("Show labels", &m_fogDebug.overlayShowLabels);
        ImGui::Checkbox("Only draw selected fog", &m_fogDebug.overlayOnlySelected);
        ImGui::SliderFloat("Overlay opacity", &m_fogDebug.overlayOpacity, 0.1f, 1.0f, "%.2f");
        ImGui::SliderFloat("Density color scale", &m_fogDebug.densityColorScale, 5.0f, 200.0f, "%.0f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Global Tweaks", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Enable All")) {
            for (auto& entry : rows) {
                entry.component->SetEnabled(true);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Disable All")) {
            for (auto& entry : rows) {
                entry.component->SetEnabled(false);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Normalize Density (0.02)")) {
            for (auto& entry : rows) {
                entry.component->SetDensity(0.02f);
            }
        }

        ImGui::SliderFloat("Density multiplier", &m_fogDebug.densityMultiplier, 0.1f, 5.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        ImGui::BeginDisabled(rows.empty());
        if (ImGui::Button("Apply Multiplier to Active")) {
            for (auto& entry : rows) {
                if (!entry.component->IsEnabled()) {
                    continue;
                }
                entry.component->SetDensity(std::max(0.0001f, entry.component->GetDensity() * m_fogDebug.densityMultiplier));
            }
        }
        ImGui::EndDisabled();
    }

    ImGui::SeparatorText("Volumes");

    if (rows.empty()) {
        ImGui::TextUnformatted("No VolumetricFogComponent instances found.");
        ImGui::End();
        return;
    }

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInner |
        ImGuiTableFlags_BordersOuter;

    if (ImGui::BeginTable("FogVolumeTable", 7, tableFlags)) {
        ImGui::TableSetupColumn("Object", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Density", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Height Falloff", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Max Distance", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Noise", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableHeadersRow();

        auto selected = m_selectedGameObject.lock();

        for (auto& entry : rows) {
            if (!entry.object || !entry.component) {
                continue;
            }

            ImGui::PushID(entry.object.get());
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            bool isSelected = (selected == entry.object);
            std::string rowLabel = fmt::format("{}##fog_row", entry.object->GetName());
            if (ImGui::Selectable(rowLabel.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selectedGameObject = entry.object;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Focus")) {
                FocusCameraOnGameObject(entry.object);
            }

            ImGui::TableSetColumnIndex(1);
            bool enabled = entry.component->IsEnabled();
            if (ImGui::Checkbox("##enabled", &enabled)) {
                entry.component->SetEnabled(enabled);
            }

            ImGui::TableSetColumnIndex(2);
            float density = entry.component->GetDensity();
            if (ImGui::DragFloat("##density", &density, 0.0005f, 0.0001f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic)) {
                entry.component->SetDensity(density);
            }

            ImGui::TableSetColumnIndex(3);
            float falloff = entry.component->GetHeightFalloff();
            if (ImGui::DragFloat("##falloff", &falloff, 0.05f, 0.1f, 10.0f)) {
                entry.component->SetHeightFalloff(falloff);
            }

            ImGui::TableSetColumnIndex(4);
            float maxDistance = entry.component->GetMaxDistance();
            if (ImGui::DragFloat("##maxDistance", &maxDistance, 1.0f, 1.0f, 500.0f)) {
                entry.component->SetMaxDistance(maxDistance);
            }

            ImGui::TableSetColumnIndex(5);
            float noiseScale = entry.component->GetNoiseScale();
            if (ImGui::DragFloat("Scale##noiseScale", &noiseScale, 0.01f, 0.05f, 5.0f)) {
                entry.component->SetNoiseScale(noiseScale);
            }
            float noiseSpeed = entry.component->GetNoiseSpeed();
            if (ImGui::DragFloat("Speed##noiseSpeed", &noiseSpeed, 0.01f, 0.0f, 5.0f)) {
                entry.component->SetNoiseSpeed(noiseSpeed);
            }

            ImGui::TableSetColumnIndex(6);
            glm::vec3 color = entry.component->GetColor();
            if (ImGui::ColorEdit3("##color", &color.x, ImGuiColorEditFlags_NoInputs)) {
                entry.component->SetColor(color);
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


