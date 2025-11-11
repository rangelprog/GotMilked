#pragma once

#include <filesystem>
#include <string>

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
};

class ConfigLoader {
public:
    static ConfigLoadResult Load(const std::filesystem::path& path);

private:
    static AppConfig CreateDefault(const std::filesystem::path& baseDir);
};

} // namespace gm::utils

