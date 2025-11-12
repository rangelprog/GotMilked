#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"

#include "gm/core/Logger.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneSerializer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace gm::debug {

void DebugMenu::AddRecentFile(const std::string& filePath) {
    if (filePath.empty()) {
        return;
    }

    m_recentFiles.erase(
        std::remove(m_recentFiles.begin(), m_recentFiles.end(), filePath),
        m_recentFiles.end());

    m_recentFiles.insert(m_recentFiles.begin(), filePath);

    if (m_recentFiles.size() > kMaxRecentFiles) {
        m_recentFiles.resize(kMaxRecentFiles);
    }

    SaveRecentFilesToDisk();
}

void DebugMenu::LoadRecentFile(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        gm::core::Logger::Warning("[DebugMenu] Recent file does not exist: {}", filePath);
        m_recentFiles.erase(
            std::remove(m_recentFiles.begin(), m_recentFiles.end(), filePath),
            m_recentFiles.end());
        SaveRecentFilesToDisk();
        return;
    }

    auto scene = m_scene.lock();
    if (!scene) {
        return;
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        gm::core::Logger::Error("[DebugMenu] Failed to open file: {}", filePath);
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string jsonStr = buffer.str();
    file.close();

    try {
        nlohmann::json sceneJson = nlohmann::json::parse(jsonStr);
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
    } catch (const nlohmann::json::exception& e) {
        gm::core::Logger::Error("[DebugMenu] JSON parse error: {}", e.what());
    }
}

void DebugMenu::LoadRecentFilesFromDisk() {
    if (!std::filesystem::exists(m_recentFilesPath)) {
        return;
    }

    std::ifstream file(m_recentFilesPath);
    if (!file.is_open()) {
        return;
    }

    m_recentFiles.clear();
    std::string line;
    while (std::getline(file, line) && m_recentFiles.size() < kMaxRecentFiles) {
        if (!line.empty() && std::filesystem::exists(line)) {
            m_recentFiles.push_back(line);
        }
    }
    file.close();
}

void DebugMenu::SaveRecentFilesToDisk() {
    std::filesystem::path path(m_recentFilesPath);
    std::filesystem::path dir = path.parent_path();
    if (!dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            gm::core::Logger::Error("[DebugMenu] Failed to create directory for recent files: {}",
                                    dir.string());
            return;
        }
    }

    std::ofstream file(m_recentFilesPath);
    if (!file.is_open()) {
        gm::core::Logger::Error("[DebugMenu] Failed to save recent files to: {}",
                                m_recentFilesPath);
        return;
    }

    for (const auto& entry : m_recentFiles) {
        file << entry << '\n';
    }
    file.close();
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
