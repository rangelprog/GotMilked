#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "EditableTerrainComponent.hpp"
#include "GameConstants.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/physics/RigidBodyComponent.hpp"
#include "gm/core/Logger.hpp"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace gm::debug {

void DebugMenu::RenderGameObjectLabels() {
    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    if (!static_cast<bool>(m_callbacks.getViewMatrix) ||
        !static_cast<bool>(m_callbacks.getProjectionMatrix) ||
        !static_cast<bool>(m_callbacks.getViewportSize)) {
        return;
    }

    glm::mat4 view = m_callbacks.getViewMatrix();
    glm::mat4 proj = m_callbacks.getProjectionMatrix();
    int viewportWidth = 0, viewportHeight = 0;
    m_callbacks.getViewportSize(viewportWidth, viewportHeight);

    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) {
        return;
    }

    const glm::mat4 viewProj = proj * view;
    const ImU32 dotColor = IM_COL32(255, 255, 255, 255);
    const ImU32 textColor = IM_COL32(255, 255, 255, 255);

    for (const auto& gameObject : scene->GetAllGameObjects()) {
        if (!gameObject || !gameObject->IsActive() || gameObject->IsDestroyed()) {
            continue;
        }

        auto transform = gameObject->GetTransform();
        if (!transform) {
            continue;
        }

        glm::vec3 worldPos = transform->GetPosition();
        glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);

        if (std::abs(clipPos.w) < 1e-6f) {
            continue;
        }

        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;

        if (ndc.z < -1.0f || ndc.z > 1.0f ||
            ndc.x < -1.0f || ndc.x > 1.0f ||
            ndc.y < -1.0f || ndc.y > 1.0f) {
            continue;
        }

        float screenX = (ndc.x + gotmilked::GameConstants::Rendering::NdcOffset) *
                        gotmilked::GameConstants::Rendering::NdcToScreenScale *
                        static_cast<float>(viewportWidth);
        float screenY = (gotmilked::GameConstants::Rendering::NdcOffset - ndc.y) *
                        gotmilked::GameConstants::Rendering::NdcToScreenScale *
                        static_cast<float>(viewportHeight);

        ImVec2 screenPos(screenX, screenY);

        drawList->AddCircleFilled(screenPos, gotmilked::GameConstants::Rendering::DotSize, dotColor, 16);

        const std::string& name = gameObject->GetName();
        if (!name.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 textPos(
                screenPos.x - textSize.x * gotmilked::GameConstants::Rendering::LabelTextOffset,
                screenPos.y - gotmilked::GameConstants::Rendering::DotSize - textSize.y - gotmilked::GameConstants::Rendering::LabelOffsetY);

            ImVec2 textMin = textPos;
            ImVec2 textMax(textPos.x + textSize.x, textPos.y + textSize.y);
            drawList->AddRectFilled(
                ImVec2(textMin.x - 2.0f, textMin.y - 2.0f),
                ImVec2(textMax.x + 2.0f, textMax.y + 2.0f),
                IM_COL32(0, 0, 0, 128), 2.0f);

            drawList->AddText(textPos, textColor, name.c_str());
        }
    }
}

void DebugMenu::RenderEditorWindow() {
    if (!ImGui::Begin("Scene Editor", &m_showInspector)) {
        ImGui::End();
        return;
    }

    ImGui::BeginChild("Hierarchy", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, 0), false);
    RenderSceneHierarchy();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Inspector", ImVec2(0, 0), false);
    RenderInspector();
    ImGui::EndChild();

    ImGui::End();
}

void DebugMenu::RenderSceneHierarchy() {
    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene available");
        return;
    }

    ImGui::Text("Scene Hierarchy");
    ImGui::Separator();

    static char searchFilter[256] = {0};
    ImGui::InputText("Search", searchFilter, sizeof(searchFilter));
    ImGui::Separator();

    auto allObjects = scene->GetAllGameObjects();
    std::string filterStr(searchFilter);
    std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                   [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });

    int visibleCount = 0;
    for (const auto& gameObject : allObjects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }

        std::string name = gameObject->GetName();
        if (name.empty()) {
            name = "Unnamed GameObject";
        }

        if (!filterStr.empty()) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
            if (lowerName.find(filterStr) == std::string::npos) {
                continue;
            }
        }

        visibleCount++;

        auto selected = m_selectedGameObject.lock();
        bool isSelected = (selected && selected == gameObject);

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        }

        std::string displayName = name;
        if (!gameObject->IsActive()) {
            displayName += " [Inactive]";
        }

        if (ImGui::Selectable(displayName.c_str(), isSelected)) {
            m_selectedGameObject = gameObject;
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }
    }

    if (visibleCount == 0 && !filterStr.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects match filter");
    } else if (visibleCount == 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObjects in scene");
    }
}

void DebugMenu::RenderInspector() {
    auto selected = m_selectedGameObject.lock();
    if (!selected) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No GameObject selected");
        ImGui::Text("Select a GameObject from the Scene Hierarchy");
        return;
    }

    char nameBuffer[256];
    const std::string& name = selected->GetName();
    std::size_t copyLen = std::min(name.length(), sizeof(nameBuffer) - 1);
    std::memcpy(nameBuffer, name.c_str(), copyLen);
    nameBuffer[copyLen] = '\0';
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        selected->SetName(nameBuffer);
    }

    bool isActive = selected->IsActive();
    if (ImGui::Checkbox("Active", &isActive)) {
        selected->SetActive(isActive);
        auto scene = m_scene.lock();
        if (scene) {
            scene->MarkActiveListsDirty();
        }
    }

    ImGui::Separator();

    auto transform = selected->GetTransform();
    if (transform) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec3 pos = transform->GetPosition();
            if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f)) {
                transform->SetPosition(pos);
            }

            glm::vec3 rot = transform->GetRotation();
            if (ImGui::DragFloat3("Rotation", glm::value_ptr(rot), 1.0f)) {
                transform->SetRotation(rot);
            }

            glm::vec3 scale = transform->GetScale();
            if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f)) {
                transform->SetScale(scale);
            }
        }
    }

    auto components = selected->GetComponents();
    for (const auto& component : components) {
        if (!component) {
            continue;
        }
        if (component == transform) {
            continue;
        }

        std::string compName = component->GetName();
        if (compName.empty()) {
            compName = "Component";
        }

        if (ImGui::CollapsingHeader(compName.c_str())) {
            if (auto meshComp = std::dynamic_pointer_cast<gm::scene::StaticMeshComponent>(component)) {
                ImGui::Text("Mesh GUID: %s", meshComp->GetMeshGuid().empty() ? "None" : meshComp->GetMeshGuid().c_str());
                ImGui::Text("Shader GUID: %s", meshComp->GetShaderGuid().empty() ? "None" : meshComp->GetShaderGuid().c_str());
                ImGui::Text("Material GUID: %s", meshComp->GetMaterialGuid().empty() ? "None" : meshComp->GetMaterialGuid().c_str());
                ImGui::Text("Has Mesh: %s", meshComp->GetMesh() ? "Yes" : "No");
                ImGui::Text("Has Shader: %s", meshComp->GetShader() ? "Yes" : "No");
                ImGui::Text("Has Material: %s", meshComp->GetMaterial() ? "Yes" : "No");
            } else if (auto rigidBody = std::dynamic_pointer_cast<gm::physics::RigidBodyComponent>(component)) {
                const char* bodyTypeNames[] = { "Static", "Dynamic" };
                int bodyType = static_cast<int>(rigidBody->GetBodyType());
                if (ImGui::Combo("Body Type", &bodyType, bodyTypeNames, 2)) {
                    rigidBody->SetBodyType(static_cast<gm::physics::RigidBodyComponent::BodyType>(bodyType));
                }

                const char* colliderNames[] = { "Plane", "Box" };
                int colliderShape = static_cast<int>(rigidBody->GetColliderShape());
                if (ImGui::Combo("Collider Shape", &colliderShape, colliderNames, 2)) {
                    rigidBody->SetColliderShape(static_cast<gm::physics::RigidBodyComponent::ColliderShape>(colliderShape));
                }

                if (rigidBody->GetColliderShape() == gm::physics::RigidBodyComponent::ColliderShape::Plane) {
                    glm::vec3 normal = rigidBody->GetPlaneNormal();
                    if (ImGui::DragFloat3("Plane Normal", glm::value_ptr(normal), 0.01f)) {
                        rigidBody->SetPlaneNormal(normal);
                    }
                    float constant = rigidBody->GetPlaneConstant();
                    if (ImGui::DragFloat("Plane Constant", &constant, 0.1f)) {
                        rigidBody->SetPlaneConstant(constant);
                    }
                } else {
                    glm::vec3 halfExtent = rigidBody->GetBoxHalfExtent();
                    if (ImGui::DragFloat3("Box Half Extent", glm::value_ptr(halfExtent), 0.1f)) {
                        rigidBody->SetBoxHalfExtent(halfExtent);
                    }
                }

                if (rigidBody->GetBodyType() == gm::physics::RigidBodyComponent::BodyType::Dynamic) {
                    float mass = rigidBody->GetMass();
                    if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.0f, 1000.0f)) {
                        rigidBody->SetMass(mass);
                    }
                }
            } else if (auto light = std::dynamic_pointer_cast<gm::LightComponent>(component)) {
                const char* lightTypeNames[] = { "Directional", "Point", "Spot" };
                int lightType = static_cast<int>(light->GetType());
                if (ImGui::Combo("Light Type", &lightType, lightTypeNames, 3)) {
                    light->SetType(static_cast<gm::LightComponent::LightType>(lightType));
                }

                bool enabled = light->IsEnabled();
                if (ImGui::Checkbox("Enabled", &enabled)) {
                    light->SetEnabled(enabled);
                }

                glm::vec3 color = light->GetColor();
                if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
                    light->SetColor(color);
                }

                float intensity = light->GetIntensity();
                if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 10.0f)) {
                    light->SetIntensity(intensity);
                }

                if (light->GetType() == gm::LightComponent::LightType::Directional ||
                    light->GetType() == gm::LightComponent::LightType::Spot) {
                    glm::vec3 direction = light->GetDirection();
                    if (ImGui::DragFloat3("Direction", glm::value_ptr(direction), 0.01f)) {
                        light->SetDirection(direction);
                    }
                }

                if (light->GetType() == gm::LightComponent::LightType::Point ||
                    light->GetType() == gm::LightComponent::LightType::Spot) {
                    glm::vec3 attenuation = light->GetAttenuation();
                    if (ImGui::DragFloat3("Attenuation", glm::value_ptr(attenuation), 0.001f, 0.0f, 10.0f)) {
                        light->SetAttenuation(attenuation.x, attenuation.y, attenuation.z);
                    }
                    ImGui::Text("(Constant, Linear, Quadratic)");
                }

                if (light->GetType() == gm::LightComponent::LightType::Spot) {
                    float innerDegrees = glm::degrees(light->GetInnerConeAngle());
                    if (ImGui::DragFloat("Inner Cone Angle", &innerDegrees, 1.0f, 0.0f, 90.0f)) {
                        light->SetInnerConeAngle(innerDegrees);
                    }
                    float outerDegrees = glm::degrees(light->GetOuterConeAngle());
                    if (ImGui::DragFloat("Outer Cone Angle", &outerDegrees, 1.0f, 0.0f, 90.0f)) {
                        light->SetOuterConeAngle(outerDegrees);
                    }
                }
            } else if (auto terrain = std::dynamic_pointer_cast<EditableTerrainComponent>(component)) {
                bool editingEnabled = terrain->IsEditingEnabled();
                if (ImGui::Checkbox("Enable Editing", &editingEnabled)) {
                    terrain->SetEditingEnabled(editingEnabled);
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Hold LMB to raise terrain.");
                ImGui::TextUnformatted("Hold RMB to lower terrain.");
                ImGui::Separator();

                float brushRadius = terrain->GetBrushRadius();
                if (ImGui::SliderFloat("Brush Radius", &brushRadius,
                        gotmilked::GameConstants::Terrain::BrushRadiusSliderMin,
                        gotmilked::GameConstants::Terrain::BrushRadiusSliderMax, "%.2f m")) {
                    terrain->SetBrushRadius(brushRadius);
                }

                float brushStrength = terrain->GetBrushStrength();
                if (ImGui::SliderFloat("Brush Strength", &brushStrength,
                        gotmilked::GameConstants::Terrain::BrushStrengthSliderMin,
                        gotmilked::GameConstants::Terrain::BrushStrengthSliderMax, "%.2f m/s")) {
                    terrain->SetBrushStrength(brushStrength);
                }
                ImGui::Separator();

                float minHeight = terrain->GetMinHeight();
                if (ImGui::SliderFloat("Min Height", &minHeight,
                        gotmilked::GameConstants::Terrain::MinHeightSliderMin,
                        gotmilked::GameConstants::Terrain::MinHeightSliderMax, "%.2f m")) {
                    terrain->SetMinHeight(minHeight);
                }

                float maxHeight = terrain->GetMaxHeight();
                if (ImGui::SliderFloat("Max Height", &maxHeight,
                        gotmilked::GameConstants::Terrain::MaxHeightSliderMin,
                        gotmilked::GameConstants::Terrain::MaxHeightSliderMax, "%.2f m")) {
                    terrain->SetMaxHeight(maxHeight);
                }

                ImGui::Separator();
                int resolution = terrain->GetResolution();
                if (ImGui::SliderInt("Resolution", &resolution, 2, 512)) {
                    terrain->SetResolution(resolution);
                    terrain->MarkMeshDirty();
                }
                ImGui::Text("Size: %.2f", terrain->GetTerrainSize());
            }
        }
    }
}

void DebugMenu::RenderSceneInfo() {
    auto scene = m_scene.lock();
    if (!scene) {
        if (ImGui::Begin("Scene Info", &m_showSceneInfo)) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No scene available");
            ImGui::End();
        }
        return;
    }

    if (!ImGui::Begin("Scene Info", &m_showSceneInfo)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Scene Name: %s", scene->GetName().c_str());
    ImGui::Text("Initialized: %s", scene->IsInitialized() ? "Yes" : "No");
    ImGui::Text("Paused: %s", scene->IsPaused() ? "Yes" : "No");

    auto allObjects = scene->GetAllGameObjects();
    std::size_t activeCount = 0;
    std::size_t componentCount = 0;
    std::unordered_map<std::string, std::size_t> componentTypes;

    for (const auto& gameObject : allObjects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }
        if (gameObject->IsActive()) {
            ++activeCount;
        }

        auto components = gameObject->GetComponents();
        componentCount += components.size();
        for (const auto& component : components) {
            if (component) {
                std::string compName = component->GetName();
                if (compName.empty()) {
                    compName = "Component";
                }
                componentTypes[compName]++;
            }
        }
    }

    ImGui::Text("GameObjects: %zu", allObjects.size());
    ImGui::Text("Active: %zu", activeCount);
    ImGui::Text("Inactive: %zu", allObjects.size() - activeCount);
    ImGui::Separator();
    ImGui::Text("Total Components: %zu", componentCount);

    if (!componentTypes.empty()) {
        ImGui::Separator();
        ImGui::Text("Component Types:");
        for (const auto& [name, count] : componentTypes) {
            ImGui::BulletText("%s: %zu", name.c_str(), count);
        }
    }

    ImGui::End();
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
