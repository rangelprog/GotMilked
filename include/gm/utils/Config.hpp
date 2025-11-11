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

struct ResourcePathConfig {
    // Shader paths (relative to assets directory)
    std::string shaderVert = "shaders/simple.vert.glsl";
    std::string shaderFrag = "shaders/simple.frag.glsl";
    
    // Texture paths (relative to assets directory)
    std::string textureGround = "textures/ground.png";
    std::string textureCow = "textures/cow.png";  // Fallback for backward compatibility
    
    // Mesh paths (relative to assets directory)
    std::string meshPlaceholder = "models/placeholder.obj";
    
    /**
     * @brief Resolve a resource path relative to the assets directory.
     * @param assetsDir Base assets directory
     * @param relativePath Relative path from assets directory
     * @return Full resolved path
     */
    std::filesystem::path ResolvePath(const std::filesystem::path& assetsDir, 
                                      const std::string& relativePath) const;
};

struct HotReloadConfig {
    bool enable = true;
    double pollIntervalSeconds = 0.5;
};

struct AppConfig {
    WindowConfig window;
    PathsConfig paths;
    ResourcePathConfig resources;
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

