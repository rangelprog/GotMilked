#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "GameConstants.hpp"
#include "EditableTerrainComponent.hpp"

#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/physics/RigidBodyComponent.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/utils/FileDialog.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/core/Logger.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace gm::debug {

void DebugMenu::RenderMenuBar() {
    if (ImGui::BeginMenu("File")) {
        m_fileMenuOpen = true;

        if (ImGui::MenuItem("Quick Save", "F5")) {
            if (m_callbacks.quickSave) {
                m_callbacks.quickSave();
            }
        }

        if (ImGui::MenuItem("Quick Load", "F9")) {
            if (m_callbacks.quickLoad) {
                m_callbacks.quickLoad();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Scene As...", "Ctrl+S")) {
            m_pendingSaveAs = true;
        }

        if (ImGui::MenuItem("Load Scene From...", "Ctrl+O")) {
            m_pendingLoad = true;
        }

        if (!m_recentFiles.empty()) {
            ImGui::Separator();
            if (ImGui::BeginMenu("Recent Files")) {
                for (size_t i = 0; i < m_recentFiles.size(); ++i) {
                    const std::string& filePath = m_recentFiles[i];
                    std::filesystem::path path(filePath);
                    std::string displayName = path.filename().string();
                    if (displayName.empty()) {
                        displayName = filePath;
                    }

                    if (ImGui::MenuItem(displayName.c_str())) {
                        LoadRecentFile(filePath);
                    }
                }
                ImGui::EndMenu();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Reload Resources")) {
            if (m_callbacks.reloadResources) {
                m_callbacks.reloadResources();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Close Menu", "F1")) {
            // Menu visibility will be toggled by the caller
        }

        ImGui::EndMenu();
    } else {
        m_fileMenuOpen = false;
    }

    if (ImGui::BeginMenu("Edit")) {
        m_editMenuOpen = true;

        ImGui::MenuItem("Undo", nullptr, false, false);
        ImGui::MenuItem("Redo", nullptr, false, false);

        ImGui::Separator();
        ImGui::MenuItem("Preferences...", nullptr, false, false);

        ImGui::EndMenu();
    } else {
        m_editMenuOpen = false;
    }

    if (ImGui::BeginMenu("View")) {
        m_optionsMenuOpen = true;

        ImGui::Checkbox("Scene Editor", &m_showInspector);
        ImGui::Checkbox("Scene Info", &m_showSceneInfo);
        ImGui::Separator();
        ImGui::Checkbox("Show GameObjects", &m_showGameObjects);

        ImGui::EndMenu();
    } else {
        m_optionsMenuOpen = false;
    }

    if (ImGui::BeginMenu("Debug")) {
        ImGui::MenuItem("Console", nullptr, &m_showDebugConsole);
        bool overlayVisible = m_overlayGetter ? m_overlayGetter() : false;
        if (ImGui::MenuItem("Tooling Overlay", nullptr, overlayVisible)) {
            overlayVisible = !overlayVisible;
            if (m_overlaySetter) {
                m_overlaySetter(overlayVisible);
            }
        }
        ImGui::EndMenu();
    }
}

void DebugMenu::RenderFileMenu() {}

void DebugMenu::RenderEditMenu() {}

void DebugMenu::RenderOptionsMenu() {}

void DebugMenu::RenderSaveAsDialog() {
    if (!ImGui::Begin("Save Scene As", &m_showSaveAsDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Enter file path:");
    ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));

    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: No scene available");
        ImGui::End();
        return;
    }

    ImGui::Separator();

    if (ImGui::Button("Save", ImVec2(120, 0))) {
        std::string filePath(m_filePathBuffer);
        if (!filePath.empty()) {
            std::filesystem::path path(filePath);
            std::filesystem::path dir = path.parent_path();
            if (!dir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
                if (ec) {
                    gm::core::Logger::Error("[DebugMenu] Failed to create directory {}: {}",
                        dir.string(), ec.message());
                }
            }

            if (gm::SceneSerializer::SaveToFile(*scene, filePath)) {
                gm::core::Logger::Info("[DebugMenu] Scene saved to: {}", filePath);
                m_showSaveAsDialog = false;
                AddRecentFile(filePath);
            } else {
                gm::core::Logger::Error("[DebugMenu] Failed to save scene to: {}", filePath);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_showSaveAsDialog = false;
    }

    ImGui::End();
}

void DebugMenu::RenderLoadDialog() {
    if (!ImGui::Begin("Load Scene From", &m_showLoadDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImGui::Text("Enter file path:");
    ImGui::InputText("##FilePath", m_filePathBuffer, sizeof(m_filePathBuffer));

    auto scene = m_scene.lock();
    if (!scene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: No scene available");
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Warning: This will replace the current scene!");
    ImGui::Separator();

    if (ImGui::Button("Load", ImVec2(120, 0))) {
        std::string filePath(m_filePathBuffer);
        if (!filePath.empty()) {
            if (std::filesystem::exists(filePath)) {
                if (gm::SceneSerializer::LoadFromFile(*scene, filePath)) {
                    gm::core::Logger::Info("[DebugMenu] Scene loaded from: {}", filePath);
                    scene->Init();
                    if (m_callbacks.onSceneLoaded) {
                        m_callbacks.onSceneLoaded();
                    }
                    m_showLoadDialog = false;
                    AddRecentFile(filePath);
                } else {
                    gm::core::Logger::Error("[DebugMenu] Failed to load scene from: {}", filePath);
                }
            } else {
                gm::core::Logger::Error("[DebugMenu] File not found: {}", filePath);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        m_showLoadDialog = false;
    }

    ImGui::End();
}

void DebugMenu::HandleSaveAs() {
    auto result = gm::utils::FileDialog::SaveFile(
        "JSON Files\0*.json\0All Files\0*.*\0",
        "json",
        m_defaultScenePath,
        m_windowHandle);

    if (result.has_value()) {
        std::string filePath = result.value();
        auto scene = m_scene.lock();
        if (scene) {
            std::filesystem::path path(filePath);
            if (std::filesystem::exists(path)) {
                std::filesystem::path dir = path.parent_path();
                std::string stem = path.stem().string();
                std::string ext = path.extension().string();

                int counter = 1;
                std::filesystem::path newPath;
                do {
                    std::string newName = stem + "_" + std::to_string(counter) + ext;
                    newPath = dir / newName;
                    counter++;
                } while (std::filesystem::exists(newPath) && counter < 1000);

                if (counter < 1000) {
                    filePath = newPath.string();
                    gm::core::Logger::Info("[DebugMenu] File exists, saving as: {}", filePath);
                } else {
                    gm::core::Logger::Error("[DebugMenu] Too many duplicate files, cannot generate unique name");
                    return;
                }
            }

            std::filesystem::path finalPath(filePath);
            std::filesystem::path dir = finalPath.parent_path();
            if (!dir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
                if (ec && !std::filesystem::exists(dir, ec)) {
                    gm::core::Logger::Error("[DebugMenu] Failed to create directory: {}", ec.message());
                    return;
                }
            }

            gm::save::SaveGameData data;
            data.sceneName = scene->GetName();

            if (m_callbacks.getCameraPosition && m_callbacks.getCameraForward && m_callbacks.getCameraFov) {
                data.cameraPosition = m_callbacks.getCameraPosition();
                data.cameraForward = m_callbacks.getCameraForward();
                data.cameraFov = m_callbacks.getCameraFov();
            }

            if (m_callbacks.getWorldTime) {
                data.worldTime = m_callbacks.getWorldTime();
            }

            std::string sceneJsonString = gm::SceneSerializer::Serialize(*scene);
            nlohmann::json sceneJson = nlohmann::json::parse(sceneJsonString);

            if (m_callbacks.getCameraPosition && m_callbacks.getCameraForward && m_callbacks.getCameraFov) {
                sceneJson["camera"] = {
                    {"position", {data.cameraPosition.x, data.cameraPosition.y, data.cameraPosition.z}},
                    {"forward", {data.cameraForward.x, data.cameraForward.y, data.cameraForward.z}},
                    {"fov", data.cameraFov}
                };
            }

            if (m_callbacks.getWorldTime) {
                sceneJson["worldTime"] = data.worldTime;
            }

            sceneJson["version"] = data.version;
            sceneJson["sceneName"] = data.sceneName;

            std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                gm::core::Logger::Error("[DebugMenu] Failed to open file for writing: {}", filePath);
                return;
            }

            file << sceneJson.dump(2);
            if (!file.good()) {
                gm::core::Logger::Error("[DebugMenu] Failed to write to file: {}", filePath);
                file.close();
                return;
            }

            file.close();
            gm::core::Logger::Info("[DebugMenu] Scene saved to: {} (includes all GameObjects and properties)", filePath);
            AddRecentFile(filePath);
        }
    } else {
#ifndef _WIN32
        m_showSaveAsDialog = true;
        std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
        std::string defaultPath = m_defaultScenePath + "scene.json";
        std::size_t copyLen = std::min(defaultPath.length(), sizeof(m_filePathBuffer) - 1);
        std::memcpy(m_filePathBuffer, defaultPath.c_str(), copyLen);
        m_filePathBuffer[copyLen] = '\0';
#endif
    }
}

void DebugMenu::HandleLoad() {
    auto result = gm::utils::FileDialog::OpenFile(
        "JSON Files\0*.json\0All Files\0*.*\0",
        m_defaultScenePath,
        m_windowHandle);

    if (result.has_value()) {
        std::string filePath = result.value();
        auto scene = m_scene.lock();
        if (scene) {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                gm::core::Logger::Error("[DebugMenu] Failed to open file for reading: {}", filePath);
                return;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            nlohmann::json sceneJson;
            try {
                sceneJson = nlohmann::json::parse(buffer.str());
            } catch (const std::exception& ex) {
                gm::core::Logger::Error("[DebugMenu] Failed to parse JSON file {}: {}", filePath, ex.what());
                return;
            }

            if (sceneJson.contains("gameObjects") && sceneJson["gameObjects"].is_array()) {
                if (!gm::SceneSerializer::Deserialize(*scene, buffer.str())) {
                    gm::core::Logger::Error("[DebugMenu] Failed to deserialize scene from: {}", filePath);
                    return;
                }

                if (sceneJson.contains("camera")) {
                    const auto& camera = sceneJson["camera"];
                    if (camera.contains("position") && camera.contains("forward") && camera.contains("fov")) {
                        auto pos = camera["position"];
                        auto fwd = camera["forward"];
                        if (pos.is_array() && pos.size() == 3 && fwd.is_array() && fwd.size() == 3) {
                            glm::vec3 cameraPos(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
                            glm::vec3 cameraFwd(fwd[0].get<float>(), fwd[1].get<float>(), fwd[2].get<float>());
                            float cameraFov = camera.value("fov", 60.0f);
                            if (m_callbacks.setCamera) {
                                m_callbacks.setCamera(cameraPos, cameraFwd, cameraFov);
                            }
                        }
                    }
                }

                if (sceneJson.contains("terrain")) {
                    const auto& terrain = sceneJson["terrain"];
                    gm::save::SaveGameData data;
                    data.terrainResolution = terrain.value("resolution", data.terrainResolution);
                    data.terrainSize = terrain.value("size", data.terrainSize);
                    data.terrainMinHeight = terrain.value("minHeight", data.terrainMinHeight);
                    data.terrainMaxHeight = terrain.value("maxHeight", data.terrainMaxHeight);

                    if (terrain.contains("heights") && terrain["heights"].is_array()) {
                        const auto& heights = terrain["heights"];
                        data.terrainHeights.clear();
                        data.terrainHeights.reserve(heights.size());
                        for (const auto& value : heights) {
                            data.terrainHeights.push_back(value.get<float>());
                        }
                    }

                    if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
                        auto terrainObject = scene->FindGameObjectByName("Terrain");
                        if (terrainObject) {
                            if (auto terrainComp = terrainObject->GetComponent<EditableTerrainComponent>()) {
                                bool ok = terrainComp->SetHeightData(
                                    data.terrainResolution,
                                    data.terrainSize,
                                    data.terrainMinHeight,
                                    data.terrainMaxHeight,
                                    data.terrainHeights);
                                if (ok) {
                                    terrainComp->MarkMeshDirty();
                                    gm::core::Logger::Info("[DebugMenu] Terrain loaded from save file");
                                }
                            }
                        }
                    }
                }

                gm::core::Logger::Info("[DebugMenu] Scene loaded from: {} (with GameObjects)", filePath);
                AddRecentFile(filePath);
                if (m_callbacks.onSceneLoaded) {
                    m_callbacks.onSceneLoaded();
                }
                return;
            }

            if (sceneJson.contains("version") && sceneJson.contains("sceneName") &&
                sceneJson.contains("camera") && sceneJson.contains("worldTime")) {
                gm::save::SaveGameData data;
                try {
                    data.version = sceneJson.value("version", data.version);
                    data.sceneName = sceneJson.value("sceneName", data.sceneName);
                    if (sceneJson.contains("camera")) {
                        const auto& camera = sceneJson["camera"];
                        if (camera.contains("position") && camera.contains("forward")) {
                            auto pos = camera["position"];
                            auto fwd = camera["forward"];
                            if (pos.is_array() && pos.size() == 3 && fwd.is_array() && fwd.size() == 3) {
                                data.cameraPosition = glm::vec3(
                                    pos[0].get<float>(),
                                    pos[1].get<float>(),
                                    pos[2].get<float>()
                                );
                                data.cameraForward = glm::vec3(
                                    fwd[0].get<float>(),
                                    fwd[1].get<float>(),
                                    fwd[2].get<float>()
                                );
                            }
                        }
                        data.cameraFov = camera.value("fov", data.cameraFov);
                    }

                    data.worldTime = sceneJson.value("worldTime", data.worldTime);

                    if (sceneJson.contains("terrain")) {
                        const auto& terrain = sceneJson["terrain"];
                        data.terrainResolution = terrain.value("resolution", data.terrainResolution);
                        data.terrainSize = terrain.value("size", data.terrainSize);
                        data.terrainMinHeight = terrain.value("minHeight", data.terrainMinHeight);
                        data.terrainMaxHeight = terrain.value("maxHeight", data.terrainMaxHeight);

                        if (terrain.contains("heights") && terrain["heights"].is_array()) {
                            const auto& heights = terrain["heights"];
                            data.terrainHeights.clear();
                            data.terrainHeights.reserve(heights.size());
                            for (const auto& value : heights) {
                                data.terrainHeights.push_back(value.get<float>());
                            }
                        }
                    }

                    if (m_callbacks.setCamera) {
                        m_callbacks.setCamera(data.cameraPosition, data.cameraForward, data.cameraFov);
                    }

                    if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
                        auto terrainObject = scene->FindGameObjectByName("Terrain");
                        if (terrainObject) {
                            if (auto terrainComp = terrainObject->GetComponent<EditableTerrainComponent>()) {
                                bool ok = terrainComp->SetHeightData(
                                    data.terrainResolution,
                                    data.terrainSize,
                                    data.terrainMinHeight,
                                    data.terrainMaxHeight,
                                    data.terrainHeights);
                                if (ok) {
                                    terrainComp->MarkMeshDirty();
                                    gm::core::Logger::Info("[DebugMenu] Terrain loaded from save file");
                                }
                            }
                        }
                    }

                    gm::core::Logger::Info("[DebugMenu] Quick Save format file loaded from: {}", filePath);
                    if (m_callbacks.onSceneLoaded) {
                        m_callbacks.onSceneLoaded();
                    }
                    return;
                } catch (const std::exception& ex) {
                    gm::core::Logger::Error("[DebugMenu] Failed to parse Quick Save format: {}", ex.what());
                    return;
                }
            }

            if (sceneJson.contains("camera") && m_callbacks.setCamera) {
                const auto& cameraJson = sceneJson["camera"];
                if (cameraJson.contains("position") && cameraJson.contains("forward")) {
                    auto pos = cameraJson["position"];
                    auto forward = cameraJson["forward"];
                    float fov = cameraJson.value("fov", gotmilked::GameConstants::Camera::DefaultFovDegrees);

                    if (pos.is_array() && pos.size() == 3 &&
                        forward.is_array() && forward.size() == 3) {
                        glm::vec3 position(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
                        glm::vec3 forwardVec(forward[0].get<float>(), forward[1].get<float>(), forward[2].get<float>());

                        m_callbacks.setCamera(position, forwardVec, fov);
                        gm::core::Logger::Info("[DebugMenu] Restored camera: pos=({:.2f}, {:.2f}, {:.2f}), fov={:.2f}",
                            position.x, position.y, position.z, fov);
                    }
                }
            }

            std::string sceneJsonStr = sceneJson.dump();
            if (gm::SceneSerializer::Deserialize(*scene, sceneJsonStr)) {
                gm::core::Logger::Info("[DebugMenu] Scene loaded from: {}", filePath);
                scene->Init();
                if (m_callbacks.onSceneLoaded) {
                    m_callbacks.onSceneLoaded();
                }
                AddRecentFile(filePath);
            } else {
                gm::core::Logger::Error("[DebugMenu] Failed to load scene from: {}", filePath);
            }
        }
    } else {
#ifndef _WIN32
        m_showLoadDialog = true;
        std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
#endif
    }
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
