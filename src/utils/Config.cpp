#include "gm/utils/Config.hpp"

#include <fstream>
#include <system_error>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

#include "gm/core/Logger.hpp"

namespace gm::utils {

namespace {

// Window size constraints
constexpr int kMinWindowWidth = 320;
constexpr int kMaxWindowWidth = 7680;
constexpr int kMinWindowHeight = 240;
constexpr int kMaxWindowHeight = 4320;
constexpr double kMinFpsTitleUpdateInterval = 0.01;
constexpr double kMaxFpsTitleUpdateInterval = 10.0;

// Hot reload constraints
constexpr double kMinPollInterval = 0.01;
constexpr double kMaxPollInterval = 60.0;

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

std::filesystem::path ConfigLoader::GetUserDocumentsPath() {
#ifdef _WIN32
    wchar_t* documentsPath = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsPath) == S_OK) {
        std::filesystem::path path(documentsPath);
        CoTaskMemFree(documentsPath);
        return path / "GotMilked";
    }
#else
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        return std::filesystem::path(homeDir) / "Documents" / "GotMilked";
    }
    // Fallback to passwd entry
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return std::filesystem::path(pw->pw_dir) / "Documents" / "GotMilked";
    }
#endif
    return std::filesystem::path();
}

AppConfig ConfigLoader::CreateDefault(const std::filesystem::path& baseDir) {
    AppConfig config{};
    config.configDirectory = baseDir;
    config.paths.assets = NormalizePath(baseDir / "assets");
    
    // Use user documents directory for saves by default
    std::filesystem::path userDocsPath = GetUserDocumentsPath();
    if (!userDocsPath.empty()) {
        config.paths.saves = NormalizePath(userDocsPath / "saves");
    } else {
        // Fallback to config directory if user docs unavailable
        config.paths.saves = NormalizePath(baseDir / "saves");
    }
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
        // User-specified saves path overrides default
        result.config.paths.saves = ResolvePath(baseDir, pathsObj["saves"].get<std::string>());
    } else {
        // Use user documents directory if not specified in config
        std::filesystem::path userDocsPath = GetUserDocumentsPath();
        if (!userDocsPath.empty()) {
            result.config.paths.saves = NormalizePath(userDocsPath / "saves");
        }
    }

    const auto hotReloadObj = json.contains("hotReload") ? json["hotReload"] : nlohmann::json::object();
    result.config.hotReload.enable = GetOrDefault<bool>(hotReloadObj, "enable", result.config.hotReload.enable);
    result.config.hotReload.pollIntervalSeconds =
        GetOrDefault<double>(hotReloadObj, "pollIntervalSeconds", result.config.hotReload.pollIntervalSeconds);

    result.config.configDirectory = baseDir;
    result.loadedFromFile = true;
    
    // Validate all config values and collect errors/warnings
    ValidateConfig(result.config, result);
    
    // Log warnings
    for (const auto& warning : result.warnings) {
        gm::core::Logger::Warning("[ConfigLoader] {}", warning);
    }
    
    // Log errors
    for (const auto& error : result.errors) {
        gm::core::Logger::Error("[ConfigLoader] {}", error);
    }
    
    if (result.loadedFromFile) {
        gm::core::Logger::Info("[ConfigLoader] Loaded config from '{}'", path.string());
    }
    
    return result;
}

void ConfigLoader::ValidateConfig(AppConfig& config, ConfigLoadResult& result) {
    ValidateWindowConfig(config.window, result);
    ValidateHotReloadConfig(config.hotReload, result);
    
    // Validate paths exist (warnings only, not errors)
    std::error_code ec;
    if (!std::filesystem::exists(config.paths.assets, ec)) {
        result.warnings.push_back(
            fmt::format("Assets directory does not exist: {}", config.paths.assets.string()));
    }
    
    // Ensure saves directory can be created (try to create it)
    std::filesystem::create_directories(config.paths.saves, ec);
    if (ec) {
        result.errors.push_back(
            fmt::format("Cannot create saves directory '{}': {}", config.paths.saves.string(), ec.message()));
    }
}

void ConfigLoader::ValidateWindowConfig(WindowConfig& window, ConfigLoadResult& result) {
    if (window.width < kMinWindowWidth || window.width > kMaxWindowWidth) {
        result.errors.push_back(
            fmt::format("Window width ({}) must be between {} and {}", 
                       window.width, kMinWindowWidth, kMaxWindowWidth));
        // Clamp to valid range
        window.width = std::clamp(window.width, kMinWindowWidth, kMaxWindowWidth);
    }
    
    if (window.height < kMinWindowHeight || window.height > kMaxWindowHeight) {
        result.errors.push_back(
            fmt::format("Window height ({}) must be between {} and {}", 
                       window.height, kMinWindowHeight, kMaxWindowHeight));
        // Clamp to valid range
        window.height = std::clamp(window.height, kMinWindowHeight, kMaxWindowHeight);
    }
    
    if (window.title.empty()) {
        result.warnings.push_back("Window title is empty, using default");
    }
    
    if (window.fpsTitleUpdateIntervalSeconds < kMinFpsTitleUpdateInterval || 
        window.fpsTitleUpdateIntervalSeconds > kMaxFpsTitleUpdateInterval) {
        result.warnings.push_back(
            fmt::format("fpsTitleUpdateIntervalSeconds ({:.3f}) should be between {:.3f} and {:.3f}, clamping",
                       window.fpsTitleUpdateIntervalSeconds, kMinFpsTitleUpdateInterval, kMaxFpsTitleUpdateInterval));
        // Clamp to valid range
        window.fpsTitleUpdateIntervalSeconds = std::clamp(window.fpsTitleUpdateIntervalSeconds, 
                                                          kMinFpsTitleUpdateInterval, 
                                                          kMaxFpsTitleUpdateInterval);
    }
}


void ConfigLoader::ValidateHotReloadConfig(HotReloadConfig& hotReload, ConfigLoadResult& result) {
    if (hotReload.enable && (hotReload.pollIntervalSeconds < kMinPollInterval || 
                             hotReload.pollIntervalSeconds > kMaxPollInterval)) {
        result.warnings.push_back(
            fmt::format("hotReload.pollIntervalSeconds ({:.3f}) should be between {:.3f} and {:.3f}, clamping",
                       hotReload.pollIntervalSeconds, kMinPollInterval, kMaxPollInterval));
        // Clamp to valid range
        hotReload.pollIntervalSeconds = std::clamp(hotReload.pollIntervalSeconds, 
                                                   kMinPollInterval, 
                                                   kMaxPollInterval);
    }
}

} // namespace gm::utils

