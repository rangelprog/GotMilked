#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "GameConstants.hpp"
#include "EditableTerrainComponent.hpp"
#include "GameResources.hpp"

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
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace {
inline void CopyToBuffer(char* dest, size_t destSize, const std::string& src) {
    if (!dest || destSize == 0) {
        return;
    }
#if defined(_MSC_VER)
    errno_t result = strncpy_s(dest, destSize, src.c_str(), _TRUNCATE);
    if (result != 0 && destSize > 0) {
        dest[destSize - 1] = '\0';
    }
#else
    std::strncpy(dest, src.c_str(), destSize - 1);
    dest[destSize - 1] = '\0';
#endif
}
} // namespace

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

        ImGui::Separator();

        if (ImGui::MenuItem("Import Model...", "Ctrl+I")) {
            m_pendingImport = true;
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
        ImGui::Checkbox("Animation Preview", &m_showAnimationDebugger);
#if defined(IMGUI_HAS_DOCK)
        if (ImGui::MenuItem("Reset Layout")) {
            m_resetDockLayout = true;
            m_showSceneExplorer = true;
            m_showSceneInfo = true;
            m_showPrefabBrowser = true;
            m_showContentBrowser = true;
            m_showAnimationDebugger = true;
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

        ImGui::Separator();
        ImGui::MenuItem("Bone Overlay", nullptr, &m_enableBoneOverlay);
        ImGui::BeginDisabled(!m_enableBoneOverlay);
        ImGui::MenuItem("Annotate Bones", nullptr, &m_showBoneNames);
        ImGui::MenuItem("Show Bones On All Objects", nullptr, &m_boneOverlayAllObjects);
        ImGui::EndDisabled();
        ImGui::MenuItem("Animation HUD", nullptr, &m_showAnimationDebugOverlay);

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

void DebugMenu::RenderImportModelDialog() {
    if (!ImGui::Begin("Import Model", &m_showImportDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    static char inputPathBuffer[512] = {0};
    static char outputDirBuffer[512] = {0};
    static char baseNameBuffer[128] = {0};

    // Initialize buffers from import settings when dialog opens or settings change
    // Check if settings have changed (for drag-and-drop)
    static std::filesystem::path lastInputPath;
    if (!m_importSettings.inputPath.empty() && m_importSettings.inputPath != lastInputPath) {
        CopyToBuffer(inputPathBuffer, sizeof(inputPathBuffer), m_importSettings.inputPath.string());
        lastInputPath = m_importSettings.inputPath;
    }

    if (!m_importSettings.outputDir.empty()) {
        std::string outputStr = m_importSettings.outputDir.string();
        if (std::string(outputDirBuffer) != outputStr) {
            CopyToBuffer(outputDirBuffer, sizeof(outputDirBuffer), outputStr);
        }
    }

    if (!m_importSettings.baseName.empty()) {
        if (std::string(baseNameBuffer) != m_importSettings.baseName) {
            CopyToBuffer(baseNameBuffer, sizeof(baseNameBuffer), m_importSettings.baseName);
        }
    }

    // Reset when dialog closes
    if (!m_showImportDialog) {
        lastInputPath.clear();
    }

    ImGui::Text("Input File:");
    ImGui::InputText("##InputPath", inputPathBuffer, sizeof(inputPathBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        auto result = gm::utils::FileDialog::OpenFile(
            "GLB/GLTF Files\0*.glb;*.gltf\0All Files\0*.*\0",
            "",
            m_windowHandle);
        if (result.has_value()) {
            CopyToBuffer(inputPathBuffer, sizeof(inputPathBuffer), result.value());
            std::filesystem::path path(result.value());
            m_importSettings.inputPath = path;
            if (baseNameBuffer[0] == '\0') {
                CopyToBuffer(baseNameBuffer, sizeof(baseNameBuffer), path.stem().string());
                m_importSettings.baseName = path.stem().string();
            }
            if (outputDirBuffer[0] == '\0' && m_gameResources) {
                std::filesystem::path defaultOutput = m_gameResources->GetAssetsDirectory() / "models" / path.stem();
                std::string outputStr = defaultOutput.string();
                CopyToBuffer(outputDirBuffer, sizeof(outputDirBuffer), outputStr);
                m_importSettings.outputDir = defaultOutput;
            }
        }
    }

    ImGui::Text("Output Directory:");
    ImGui::InputText("##OutputDir", outputDirBuffer, sizeof(outputDirBuffer));
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_importSettings.outputDir = outputDirBuffer;
    }

    ImGui::Text("Base Name:");
    ImGui::InputText("##BaseName", baseNameBuffer, sizeof(baseNameBuffer));

    ImGui::Separator();

    ImGui::Checkbox("Generate Prefab", &m_importSettings.generatePrefab);
    ImGui::Checkbox("Overwrite Existing", &m_importSettings.overwriteExisting);

    ImGui::Separator();

    if (m_importInProgress) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Importing...");
        if (!m_importStatusMessage.empty()) {
            ImGui::TextWrapped("%s", m_importStatusMessage.c_str());
        }
    } else {
        if (!m_importStatusMessage.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", m_importStatusMessage.c_str());
        }

        if (ImGui::Button("Import", ImVec2(120, 0))) {
            std::string inputPath(inputPathBuffer);
            std::string outputDir(outputDirBuffer);
            std::string baseName(baseNameBuffer);

            if (inputPath.empty()) {
                m_importStatusMessage = "Error: Input file path is required";
            } else if (!std::filesystem::exists(inputPath)) {
                m_importStatusMessage = "Error: Input file does not exist";
            } else if (outputDir.empty()) {
                m_importStatusMessage = "Error: Output directory is required";
            } else if (baseName.empty()) {
                m_importStatusMessage = "Error: Base name is required";
            } else {
                m_importSettings.inputPath = inputPath;
                m_importSettings.outputDir = outputDir;
                m_importSettings.baseName = baseName;
                m_importInProgress = true;
                m_importStatusMessage.clear();

                bool success = ExecuteModelImport(
                    m_importSettings.inputPath,
                    m_importSettings.outputDir,
                    m_importSettings.baseName);

                m_importInProgress = false;
                if (success) {
                    m_importStatusMessage = "Import completed successfully!";
                    if (m_applyResourcesCallback) {
                        m_applyResourcesCallback();
                    }
                    // Clear buffers for next import
                    inputPathBuffer[0] = '\0';
                    outputDirBuffer[0] = '\0';
                    baseNameBuffer[0] = '\0';
                    m_importSettings = ImportSettings{};
                } else {
                    m_importStatusMessage = "Import failed. Check console for details.";
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            m_showImportDialog = false;
            m_importStatusMessage.clear();
            inputPathBuffer[0] = '\0';
            outputDirBuffer[0] = '\0';
            baseNameBuffer[0] = '\0';
            m_importSettings = ImportSettings{};
        }
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
