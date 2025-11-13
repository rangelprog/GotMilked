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
#include "gm/gameplay/FlyCameraController.hpp"
#include "gm/save/SaveVersion.hpp"

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

        ImGui::MenuItem("Scene Explorer", nullptr, &m_showSceneExplorer);

        ImGui::EndMenu();
    } else {
        m_editMenuOpen = false;
    }

    if (ImGui::BeginMenu("View")) {
        m_optionsMenuOpen = true;

        ImGui::Checkbox("Scene Info", &m_showSceneInfo);
        ImGui::Checkbox("Prefab Browser", &m_showPrefabBrowser);
        ImGui::Checkbox("Content Browser", &m_showContentBrowser);
#if defined(IMGUI_HAS_DOCK)
        if (ImGui::MenuItem("Reset Layout")) {
            m_resetDockLayout = true;
            m_showSceneExplorer = true;
            m_showSceneInfo = true;
            m_showPrefabBrowser = true;
            m_showContentBrowser = true;
        }
#endif

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
                BeginSceneReload();
                bool loadSuccess = gm::SceneSerializer::LoadFromFile(*scene, filePath);
                if (loadSuccess) {
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
                EndSceneReload();
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

            sceneJson["version"] = gm::save::SaveVersionToJson(data.version);
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

    if (!result.has_value()) {
#ifndef _WIN32
        m_showLoadDialog = true;
        std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
#endif
        return;
    }

    const std::string filePath = result.value();
    auto scene = m_scene.lock();
    if (!scene) {
        gm::core::Logger::Error("[DebugMenu] HandleLoad called with no active scene (file: {})", filePath);
        return;
    }

    if (!std::filesystem::exists(filePath)) {
        gm::core::Logger::Error("[DebugMenu] Selected file does not exist: {}", filePath);
        return;
    }

    std::ifstream file(filePath);
    if (!file.is_open()) {
        gm::core::Logger::Error("[DebugMenu] Failed to open file for reading: {}", filePath);
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    const std::string jsonString = buffer.str();
    if (jsonString.empty()) {
        gm::core::Logger::Error("[DebugMenu] Selected file is empty: {}", filePath);
        return;
    }

    nlohmann::json sceneJson;
    try {
        sceneJson = nlohmann::json::parse(jsonString);
    } catch (const std::exception& ex) {
        gm::core::Logger::Error("[DebugMenu] Failed to parse JSON file {}: {}", filePath, ex.what());
        return;
    }
    if (!sceneJson.is_object()) {
        gm::core::Logger::Error("[DebugMenu] Scene JSON root is not an object: {}", filePath);
        return;
    }

    gm::core::Logger::Info("[DebugMenu] Loading scene from '{}'", filePath);
    if (sceneJson.contains("gameObjects")) {
        gm::core::Logger::Info("[DebugMenu] JSON has {} gameObjects", sceneJson["gameObjects"].size());
    }

    std::string quickLoadPath;
    if (std::filesystem::exists("apps/GotMilked/config.json")) {
        try {
            std::ifstream cfg("apps/GotMilked/config.json");
            nlohmann::json cfgJson;
            cfg >> cfgJson;
            if (cfgJson.contains("lastQuickLoad") && cfgJson["lastQuickLoad"].is_string()) {
                quickLoadPath = cfgJson["lastQuickLoad"].get<std::string>();
            }
        } catch (...) {}
    }
    if (!quickLoadPath.empty()) {
        gm::core::Logger::Info("[DebugMenu] Current quick-load path: {}", quickLoadPath);
    }
    gm::core::Logger::Info("[DebugMenu] m_lastQuickLoadPath before load: {}", m_lastQuickLoadPath.c_str());

    BeginSceneReload();
    const bool restoreSuccess = gm::SceneSerializer::Deserialize(*scene, jsonString);
    EndSceneReload();

    if (!restoreSuccess) {
        gm::core::Logger::Error("[DebugMenu] SceneSerializer::Deserialize failed for {}", filePath);
        return;
    }

    const auto& objects = scene->GetAllGameObjects();
    gm::core::Logger::Info("[DebugMenu] Scene deserialize completed; object count: {}", objects.size());
    for (const auto& obj : objects) {
        if (!obj) {
            gm::core::Logger::Error("[DebugMenu] Scene contains null GameObject after deserialize");
            continue;
        }
        const std::string& objName = obj->GetName();
        if (objName.empty()) {
            gm::core::Logger::Error("[DebugMenu] Scene contains GameObject with empty name after deserialize (address: {})",
                static_cast<const void*>(obj.get()));
        } else {
            gm::core::Logger::Debug("[DebugMenu] GameObject: {}", objName);
        }

        auto components = obj->GetComponents();
        gm::core::Logger::Debug("[DebugMenu]   Components: {}", components.size());
        for (const auto& comp : components) {
            if (!comp) {
                gm::core::Logger::Error("[DebugMenu]   Null component on GameObject '{}'", objName);
                continue;
            }
            const std::string& compName = comp->GetName();
            if (compName.empty()) {
                gm::core::Logger::Error("[DebugMenu]   Component with empty name on '{}', typeid {}", objName,
                    typeid(*comp).name());
            } else {
                gm::core::Logger::Debug("[DebugMenu]     {}", compName);
            }
        }
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
    m_lastQuickLoadPath = filePath;
    gm::core::Logger::Info("[DebugMenu] m_lastQuickLoadPath updated to {}", m_lastQuickLoadPath.c_str());
    if (m_callbacks.onSceneLoaded) {
        m_callbacks.onSceneLoaded();
    }
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
