#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace gm::utils {

struct WindowConfig {
    int width = 1280;
    int height = 720;
    std::string title = "GotMilked";
    bool vsync = true;
    bool depthTest = true;
    bool showFpsInTitle = true;
    double fpsTitleUpdateIntervalSeconds = 0.5;
};

struct PathsConfig {
    std::filesystem::path assets;
    std::filesystem::path saves;
};

struct HotReloadConfig {
    bool enable = true;
    double pollIntervalSeconds = 0.5;
};

struct AppConfig {
    WindowConfig window;
    PathsConfig paths;
    std::filesystem::path configDirectory;
    HotReloadConfig hotReload;
};

struct ConfigLoadResult {
    AppConfig config;
    bool loadedFromFile = false;
    std::vector<std::string> errors;      // Critical errors that should prevent startup
    std::vector<std::string> warnings;    // Non-critical issues that should be logged
    
    bool HasErrors() const { return !errors.empty(); }
    bool HasWarnings() const { return !warnings.empty(); }
};

class ConfigLoader {
public:
    static ConfigLoadResult Load(const std::filesystem::path& path);
    
    /**
     * @brief Get the default user documents directory for saves/logs.
     * @return Path to user documents/GotMilked directory, or empty path if unavailable
     */
    static std::filesystem::path GetUserDocumentsPath();

private:
    static AppConfig CreateDefault(const std::filesystem::path& baseDir);
    static void ValidateConfig(AppConfig& config, ConfigLoadResult& result);
    static void ValidateWindowConfig(WindowConfig& window, ConfigLoadResult& result);
    static void ValidateHotReloadConfig(HotReloadConfig& hotReload, ConfigLoadResult& result);
};

} // namespace gm::utils

