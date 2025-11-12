#include "gm/utils/Config.hpp"

#include <fstream>
#include <system_error>
#include <nlohmann/json.hpp>

#include "gm/core/Logger.hpp"

namespace gm::utils {

namespace {

static std::filesystem::path NormalizePath(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        normalized = std::filesystem::absolute(path);
    }
    return normalized.lexically_normal();
}

static std::filesystem::path ResolvePath(const std::filesystem::path& baseDir,
                                         const std::string& value) {
    if (value.empty()) {
        return NormalizePath(baseDir);
    }
    std::filesystem::path raw(value);
    if (raw.is_relative()) {
        return NormalizePath(baseDir / raw);
    }
    return NormalizePath(raw);
}

template <typename T>
static T GetOrDefault(const nlohmann::json& obj, const char* key, const T& fallback) {
    if (!obj.is_object()) {
        return fallback;
    }
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
        return fallback;
    }
    try {
        return it->get<T>();
    } catch (const nlohmann::json::exception& e) {
        gm::core::Logger::Warning("[ConfigLoader] Failed to parse key '{}': {}", key, e.what());
        return fallback;
    }
}

} // namespace

std::filesystem::path gm::utils::ResourcePathConfig::ResolvePath(
    const std::filesystem::path& assetsDir,
    const std::string& relativePath) const {
    if (relativePath.empty()) {
        return NormalizePath(assetsDir);
    }
    std::filesystem::path raw(relativePath);
    if (raw.is_absolute()) {
        return NormalizePath(raw);
    }
    return NormalizePath(assetsDir / raw);
}

AppConfig ConfigLoader::CreateDefault(const std::filesystem::path& baseDir) {
    AppConfig config{};
    config.configDirectory = baseDir;
    config.paths.assets = NormalizePath(baseDir / "assets");
    config.paths.saves = NormalizePath(baseDir / "saves");
    return config;
}

ConfigLoadResult ConfigLoader::Load(const std::filesystem::path& path) {
    const std::filesystem::path baseDir = path.empty() ? std::filesystem::current_path()
                                                       : path.parent_path();

    ConfigLoadResult result;
    result.config = CreateDefault(baseDir);

    std::error_code ec;
    if (path.empty() || !std::filesystem::exists(path, ec)) {
        gm::core::Logger::Warning(
            "[ConfigLoader] Config file '{}' not found, using defaults",
            path.empty() ? "<none>" : path.string());
        return result;
    }

    std::ifstream file(path);
    if (!file) {
        gm::core::Logger::Error("[ConfigLoader] Failed to open config file '{}'", path.string());
        return result;
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const nlohmann::json::exception& e) {
        gm::core::Logger::Error("[ConfigLoader] Failed to parse JSON '{}': {}",
                                path.string(), e.what());
        return result;
    }

    const auto windowObj = json.contains("window") ? json["window"] : nlohmann::json::object();
    result.config.window.width = GetOrDefault<int>(windowObj, "width", result.config.window.width);
    result.config.window.height = GetOrDefault<int>(windowObj, "height", result.config.window.height);
    result.config.window.title = GetOrDefault<std::string>(windowObj, "title", result.config.window.title);
    result.config.window.vsync = GetOrDefault<bool>(windowObj, "vsync", result.config.window.vsync);
    result.config.window.depthTest = GetOrDefault<bool>(windowObj, "depthTest", result.config.window.depthTest);
    result.config.window.showFpsInTitle = GetOrDefault<bool>(windowObj, "showFpsInTitle", result.config.window.showFpsInTitle);
    result.config.window.fpsTitleUpdateIntervalSeconds =
        GetOrDefault<double>(windowObj,
                             "fpsTitleUpdateIntervalSeconds",
                             result.config.window.fpsTitleUpdateIntervalSeconds);

    const auto pathsObj = json.contains("paths") ? json["paths"] : nlohmann::json::object();
    if (pathsObj.contains("assets") && pathsObj["assets"].is_string()) {
        result.config.paths.assets = ResolvePath(baseDir, pathsObj["assets"].get<std::string>());
    }
    if (pathsObj.contains("saves") && pathsObj["saves"].is_string()) {
        result.config.paths.saves = ResolvePath(baseDir, pathsObj["saves"].get<std::string>());
    }

    // Load resource path configuration
    const auto resourcesObj = json.contains("resources") ? json["resources"] : nlohmann::json::object();
    result.config.resources.shaderVert = GetOrDefault<std::string>(resourcesObj, "shaderVert", result.config.resources.shaderVert);
    result.config.resources.shaderFrag = GetOrDefault<std::string>(resourcesObj, "shaderFrag", result.config.resources.shaderFrag);
    result.config.resources.textureGround = GetOrDefault<std::string>(resourcesObj, "textureGround", result.config.resources.textureGround);
    result.config.resources.textureCow = GetOrDefault<std::string>(resourcesObj, "textureCow", result.config.resources.textureCow);
    result.config.resources.meshPlaceholder = GetOrDefault<std::string>(resourcesObj, "meshPlaceholder", result.config.resources.meshPlaceholder);

    const auto hotReloadObj = json.contains("hotReload") ? json["hotReload"] : nlohmann::json::object();
    result.config.hotReload.enable = GetOrDefault<bool>(hotReloadObj, "enable", result.config.hotReload.enable);
    result.config.hotReload.pollIntervalSeconds =
        GetOrDefault<double>(hotReloadObj, "pollIntervalSeconds", result.config.hotReload.pollIntervalSeconds);
    if (result.config.hotReload.pollIntervalSeconds <= 0.0) {
        gm::core::Logger::Warning("[ConfigLoader] hotReload.pollIntervalSeconds ({:.3f}) must be positive; clamping to 0.1",
                                  result.config.hotReload.pollIntervalSeconds);
        result.config.hotReload.pollIntervalSeconds = 0.1;
    }

    result.config.configDirectory = baseDir;
    result.loadedFromFile = true;
    gm::core::Logger::Info("[ConfigLoader] Loaded config from '{}'", path.string());
    return result;
}

} // namespace gm::utils

