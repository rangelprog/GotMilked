#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "Game.hpp"
#include "GameBootstrapper.hpp"
#include "GameRenderer.hpp"
#include "DebugToolingController.hpp"
#include "SceneResourceController.hpp"
#include "GameShutdownController.hpp"
#include "ToolingFacade.hpp"
#include "NarrativeScriptingLog.hpp"
#include "EventRouter.hpp"
#include "GameSceneHelpers.hpp"
#include "GameConstants.hpp"
#include "GameEvents.hpp"
#include "SceneSerializerExtensions.hpp"
#include "GameLoopController.hpp"
#include "WeatherService.hpp"
#include "ScriptingHooks.hpp"
#include "gameplay/FlyCameraController.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/PrefabLibrary.hpp"
#include "gm/utils/Profiler.hpp"
#include "gm/core/GameApp.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <algorithm>
#include <utility>
#include <cmath>
#include <glm/common.hpp>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>
#include <typeinfo>
#include <fmt/format.h>

#include "gm/rendering/Camera.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneManager.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/physics/PhysicsWorld.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/CascadeShadowMap.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/InputBindings.hpp"
#include "gm/core/input/InputManager.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/save/SaveDiff.hpp"
#include "gm/save/SaveVersion.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gameplay/CameraRigSystem.hpp"
#include "gameplay/CameraRigComponent.hpp"
#include "gameplay/QuestTriggerComponent.hpp"
#include "gameplay/QuestTriggerSystem.hpp"
#include "gameplay/DialogueTriggerComponent.hpp"
#include "gameplay/DialogueTriggerSystem.hpp"
#if GM_DEBUG_TOOLS
#include "EditableTerrainComponent.hpp"
#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "DebugHudController.hpp"
#include "gm/debug/GridRenderer.hpp"

using gm::debug::EditableTerrainComponent;
#endif
#include <imgui.h>
#include <glad/glad.h>


Game::Game(const gm::utils::AppConfig& config)
    : m_config(config),
      m_assetsDir(config.paths.assets),
      m_weatherService(std::make_shared<WeatherService>()),
      m_scriptingHooks(std::make_shared<ScriptingHooks>()),
      m_narrativeLog(std::make_shared<NarrativeScriptingLog>(m_scriptingHooks)) {
    WeatherService::SetGlobalInstance(m_weatherService);
#if GM_DEBUG_TOOLS
    m_debugHud = std::make_unique<gm::debug::DebugHudController>();
    m_terrainEditingSystem = std::make_shared<gm::debug::TerrainEditingSystem>();
#endif
    m_bootstrapper = std::make_unique<GameBootstrapper>(*this);
    m_renderer = std::make_unique<GameRenderer>(*this);
    m_toolingFacade = std::make_unique<ToolingFacade>(*this);
    m_cameraRigSystem = std::make_shared<gm::gameplay::CameraRigSystem>();
    m_questSystem = std::make_shared<gm::gameplay::QuestTriggerSystem>();
    m_dialogueSystem = std::make_shared<gm::gameplay::DialogueTriggerSystem>();
    m_scriptingHooks = std::make_shared<ScriptingHooks>();
    m_resources.SetIssueReporter([this](const std::string& message, bool isError) {
        if (!m_toolingFacade) {
            return;
        }
        const std::string formatted = isError
            ? fmt::format("Resource error: {}", message)
            : fmt::format("Resource warning: {}", message);
        m_toolingFacade->AddNotification(formatted);
    });
    m_debugTooling = std::make_unique<DebugToolingController>(*this);
    m_sceneResources = std::make_unique<SceneResourceController>(*this);
    m_shutdownController = std::make_unique<GameShutdownController>(*this);
    m_eventRouter = std::make_unique<EventRouter>();
    m_loopController = std::make_unique<GameLoopController>(*this);
    m_contentDatabase = std::make_unique<gm::content::ContentDatabase>();
    m_contentDatabase->SetNotificationCallback([this](const std::string& message, bool isError) {
        if (m_toolingFacade) {
            m_toolingFacade->AddNotification(message);
            return;
        }
        if (isError) {
            gm::core::Logger::Error("[Content] {}", message);
        } else {
            gm::core::Logger::Info("[Content] {}", message);
        }
    });

    m_lastCaptureWeatherProfile = m_weatherState.activeProfile;
    m_lastCaptureWetness = m_weatherState.surfaceWetness;
    m_lastCaptureSunElevation = m_sunMoonState.sunElevationDeg;
}

Game::~Game() {
    if (m_contentDatabase) {
        m_contentDatabase->Shutdown();
    }
    WeatherService::SetGlobalInstance(nullptr);
}

bool Game::Init(GLFWwindow* window, gm::SceneManager& sceneManager) {
    if (!m_bootstrapper) {
        m_bootstrapper = std::make_unique<GameBootstrapper>(*this);
    }
    if (!m_debugTooling) {
        m_debugTooling = std::make_unique<DebugToolingController>(*this);
    }
    return m_bootstrapper->Initialize(window, sceneManager);
}

void Game::BindAppContext(gm::core::GameAppContext& context) {
    m_appContext = &context;
    if (context.setVSyncEnabled) {
        context.setVSyncEnabled(m_vsyncEnabled);
    }
}

bool Game::SetupLogging() {
    // Use user documents directory for logs (same parent as saves)
    std::filesystem::path logDir;
    std::filesystem::path userDocsPath = gm::utils::ConfigLoader::GetUserDocumentsPath();
    if (!userDocsPath.empty()) {
        logDir = userDocsPath / "logs";
    } else {
        // Fallback to saves directory if user docs unavailable
        logDir = m_config.paths.saves / "logs";
    }
    
    std::error_code ec;
    std::filesystem::create_directories(logDir, ec);
    if (ec) {
        gm::core::Logger::Error("[Game] Failed to create log directory '{}': {}", logDir.string(), ec.message());
        return false;
    }
    
    const std::filesystem::path logPath = logDir / "game.log";
    gm::core::Logger::SetLogFile(logPath);
    gm::core::Logger::Info("[Game] Logging to {}", logPath.string());
#ifdef GM_DEBUG
    gm::core::Logger::SetDebugEnabled(true);
#endif
    return true;
}

void Game::RequestExit() const {
    if (m_appContext && m_appContext->requestExit) {
        m_appContext->requestExit();
    } else if (m_window) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void Game::SetVSyncEnabled(bool enabled) {
    m_vsyncEnabled = enabled;
    if (m_appContext && m_appContext->setVSyncEnabled) {
        m_appContext->setVSyncEnabled(enabled);
    } else if (m_window) {
        glfwSwapInterval(enabled ? 1 : 0);
    }
}

bool Game::IsVSyncEnabled() const {
    if (m_appContext && m_appContext->isVSyncEnabled) {
        return m_appContext->isVSyncEnabled();
    }
    return m_vsyncEnabled;
}

bool Game::SetupPhysics() {
    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (!physics.IsInitialized()) {
        try {
            physics.Init();
        } catch (const std::exception& ex) {
            gm::core::Logger::Error("[Game] Failed to initialize physics: {}", ex.what());
            return false;
        } catch (...) {
            gm::core::Logger::Error("[Game] Failed to initialize physics: unknown error");
            return false;
        }
    }
    
    if (!physics.IsInitialized()) {
        gm::core::Logger::Error("[Game] Physics initialization completed but IsInitialized() returned false");
        return false;
    }
    
    return true;
}

bool Game::SetupRendering() {
    if (!m_resources.Load(m_assetsDir)) {
        gm::core::Logger::Error("[Game] Failed to load resources from {}", m_assetsDir.string());
        return false;
    }
    m_assetsDir = m_resources.GetAssetsDirectory();
    if (m_contentDatabase) {
        m_contentDatabase->Initialize(m_assetsDir);
    }

    LoadCelestialConfig();
    LoadWeatherProfiles();
    gm::SceneSerializerExtensions::RegisterSerializers();
    m_camera = std::make_unique<gm::Camera>();
    return true;
}

void Game::LoadCelestialConfig() {
    gm::scene::CelestialConfig config{};
    const auto configPath = (m_assetsDir / "config" / "celestial.json").lexically_normal();
    std::error_code existsEc;
    if (!std::filesystem::exists(configPath, existsEc)) {
        if (existsEc) {
            gm::core::Logger::Warning("[Game] Unable to check celestial config '{}': {}", configPath.string(), existsEc.message());
        } else {
            gm::core::Logger::Info("[Game] Celestial config '{}' not found; using defaults", configPath.string());
        }
        m_timeOfDayController.SetConfig(config);
        m_sunMoonState = m_timeOfDayController.Evaluate();
        SyncWeatherService();
        return;
    }

    std::ifstream file(configPath);
    if (!file.is_open()) {
        gm::core::Logger::Warning("[Game] Failed to open celestial config '{}'; using defaults", configPath.string());
        m_timeOfDayController.SetConfig(config);
        m_sunMoonState = m_timeOfDayController.Evaluate();
        SyncWeatherService();
        return;
    }

    nlohmann::json json = nlohmann::json::parse(file, nullptr, false);
    if (!json.is_object()) {
        gm::core::Logger::Warning("[Game] Celestial config '{}' is malformed; using defaults", configPath.string());
        m_timeOfDayController.SetConfig(config);
        m_sunMoonState = m_timeOfDayController.Evaluate();
        SyncWeatherService();
        return;
    }

    config.latitudeDeg = json.value("latitudeDeg", config.latitudeDeg);
    config.axialTiltDeg = json.value("axialTiltDeg", config.axialTiltDeg);
    config.dayLengthSeconds = std::max(1.0f, json.value("dayLengthSeconds", config.dayLengthSeconds));
    config.timeOffsetSeconds = json.value("timeOffsetSeconds", config.timeOffsetSeconds);
    config.moonPhaseOffsetSeconds = json.value("moonPhaseOffsetSeconds", config.moonPhaseOffsetSeconds);
    config.moonlightIntensity = json.value("moonlightIntensity", config.moonlightIntensity);
    config.turbidity = json.value("turbidity", config.turbidity);
    config.exposure = json.value("exposure", config.exposure);
    config.airDensity = json.value("airDensity", config.airDensity);
    if (auto albedoIt = json.find("groundAlbedo"); albedoIt != json.end() && albedoIt->is_array() && albedoIt->size() == 3) {
        config.groundAlbedo = glm::vec3((*albedoIt)[0].get<float>(),
                                        (*albedoIt)[1].get<float>(),
                                        (*albedoIt)[2].get<float>());
    }
    config.useGradientSky = json.value("useGradientSky", config.useGradientSky);
    config.middayLux = json.value("middayLux", config.middayLux);
    config.exposureReferenceLux = json.value("exposureReferenceLux", config.exposureReferenceLux);
    config.exposureTargetEv = json.value("exposureTargetEv", config.exposureTargetEv);
    config.exposureBias = json.value("exposureBias", config.exposureBias);
    config.exposureSmoothing = json.value("exposureSmoothing", config.exposureSmoothing);
    config.exposureMin = json.value("exposureMin", config.exposureMin);
    config.exposureMax = json.value("exposureMax", config.exposureMax);

    m_timeOfDayController.SetConfig(config);

    const float startTimeHours = json.value("startTimeHours", 6.0f);
    const float clampedHours = std::clamp(startTimeHours / 24.0f, 0.0f, 1.0f);
    m_timeOfDayController.SetTimeSeconds(clampedHours * config.dayLengthSeconds);
    m_sunMoonState = m_timeOfDayController.Evaluate();
    m_exposureAccumulator = config.exposureBias;
    m_sunMoonState.exposureCompensation = m_exposureAccumulator;

    gm::core::Logger::Info("[Game] Loaded celestial config '{}': latitude={}°, tilt={}°, dayLength={}s",
                           configPath.string(),
                           config.latitudeDeg,
                           config.axialTiltDeg,
                           config.dayLengthSeconds);
    SyncWeatherService();
}

bool Game::LoadWeatherProfiles() {
    m_weatherProfiles.clear();
    m_weatherForecast = WeatherForecast{};
    const auto configPath = (m_assetsDir / "config" / "weather_profiles.json").lexically_normal();
    std::error_code existsEc;
    if (!std::filesystem::exists(configPath, existsEc)) {
        if (existsEc) {
            gm::core::Logger::Warning("[Game] Unable to check weather profile config '{}': {}", configPath.string(), existsEc.message());
        } else {
            gm::core::Logger::Info("[Game] Weather profile config '{}' not found; using defaults", configPath.string());
        }
        WeatherProfile fallback{};
        fallback.name = "default";
        fallback.spawnMultiplier = 0.0f;
        fallback.tint = glm::vec3(1.0f);
        m_weatherProfiles.emplace(fallback.name, fallback);
        m_weatherState = WeatherState{};
        WeatherForecastEntry defaultForecast{};
        defaultForecast.profile = m_weatherState.activeProfile;
        defaultForecast.startHour = std::clamp(m_timeOfDayController.GetNormalizedTime() * 24.0f, 0.0f, 24.0f);
        defaultForecast.durationHours = 6.0f;
        defaultForecast.description = "Default conditions";
        m_weatherForecast.generatedAtNormalizedTime = m_timeOfDayController.GetNormalizedTime();
        m_weatherForecast.entries.push_back(defaultForecast);
        if (m_weatherService) {
            m_weatherService->SetForecast(m_weatherForecast);
        }
        SyncWeatherService();
        return true;
    }

    std::ifstream file(configPath);
    if (!file.is_open()) {
        gm::core::Logger::Warning("[Game] Failed to open weather profile config '{}'", configPath.string());
        return false;
    }

    nlohmann::json json = nlohmann::json::parse(file, nullptr, false);
    if (!json.is_object()) {
        gm::core::Logger::Warning("[Game] Weather profile config '{}' is malformed", configPath.string());
        return false;
    }

    if (auto profilesIt = json.find("profiles"); profilesIt != json.end() && profilesIt->is_array()) {
        for (const auto& entry : *profilesIt) {
            if (!entry.is_object()) {
                continue;
            }
            WeatherProfile profile = ParseProfile(entry);
            if (!profile.name.empty()) {
                m_weatherProfiles[profile.name] = profile;
            }
        }
    }

    if (m_weatherProfiles.empty()) {
        WeatherProfile fallback{};
        fallback.name = "default";
        fallback.spawnMultiplier = 0.0f;
        fallback.tint = glm::vec3(1.0f);
        fallback.surfaceTint = glm::vec3(1.0f);
        m_weatherProfiles.emplace(fallback.name, fallback);
    }

    const std::string defaultProfile = "default";
    std::string quality = json.value("quality", "high");
    if (quality == "low") {
        m_weatherQuality = WeatherQuality::Low;
    } else if (quality == "medium") {
        m_weatherQuality = WeatherQuality::Medium;
    } else {
        m_weatherQuality = WeatherQuality::High;
    }

    if (auto initialIt = json.find("initialState"); initialIt != json.end() && initialIt->is_object()) {
        m_weatherState = ParseInitialWeather(*initialIt);
    } else {
        m_weatherState = WeatherState{};
    }

    if (!m_weatherProfiles.contains(m_weatherState.activeProfile)) {
        m_weatherState.activeProfile = defaultProfile;
    }

    const WeatherProfile& activeProfile = ResolveWeatherProfile(m_weatherState.activeProfile);
    m_weatherState.surfaceWetness = activeProfile.surfaceWetness;
    m_weatherState.puddleAmount = activeProfile.puddleAmount;
    m_weatherState.surfaceDarkening = activeProfile.surfaceDarkening;
    m_weatherState.surfaceTint = activeProfile.surfaceTint;

    m_weatherForecast = ParseForecast(json);
    if (m_weatherForecast.entries.empty()) {
        WeatherForecastEntry entry{};
        entry.profile = m_weatherState.activeProfile;
        entry.startHour = std::clamp(m_timeOfDayController.GetNormalizedTime() * 24.0f, 0.0f, 24.0f);
        entry.durationHours = 6.0f;
        entry.description = "Continuation of current conditions";
        m_weatherForecast.entries.push_back(entry);
    }
    m_weatherForecast.generatedAtNormalizedTime = m_timeOfDayController.GetNormalizedTime();
    if (m_weatherService) {
        m_weatherService->SetForecast(m_weatherForecast);
    }

    gm::core::Logger::Info("[Game] Loaded {} weather profiles from '{}'", m_weatherProfiles.size(), configPath.string());
    BroadcastWeatherEvent();
    SyncWeatherService();
    return true;
}

WeatherProfile Game::ParseProfile(const nlohmann::json& entry) const {
    WeatherProfile profile{};
    profile.name = entry.value("name", std::string{});
    profile.spawnMultiplier = entry.value("spawnMultiplier", profile.spawnMultiplier);
    profile.speedMultiplier = entry.value("speedMultiplier", profile.speedMultiplier);
    profile.sizeMultiplier = entry.value("sizeMultiplier", profile.sizeMultiplier);
    if (auto tintIt = entry.find("tint"); tintIt != entry.end() && tintIt->is_array() && tintIt->size() == 3) {
        profile.tint = glm::vec3((*tintIt)[0].get<float>(),
                                 (*tintIt)[1].get<float>(),
                                 (*tintIt)[2].get<float>());
    }
    profile.surfaceWetness = entry.value("surfaceWetness", profile.surfaceWetness);
    profile.puddleAmount = entry.value("puddleAmount", profile.puddleAmount);
    profile.surfaceDarkening = entry.value("surfaceDarkening", profile.surfaceDarkening);
    if (auto surfaceTint = entry.find("surfaceTint"); surfaceTint != entry.end() && surfaceTint->is_array() && surfaceTint->size() == 3) {
        profile.surfaceTint = glm::vec3((*surfaceTint)[0].get<float>(),
                                        (*surfaceTint)[1].get<float>(),
                                        (*surfaceTint)[2].get<float>());
    } else {
        profile.surfaceTint = glm::vec3(1.0f);
    }
    return profile;
}

WeatherState Game::ParseInitialWeather(const nlohmann::json& data) const {
    WeatherState state{};
    state.activeProfile = data.value("profile", state.activeProfile);
    if (auto windIt = data.find("windDirection"); windIt != data.end() && windIt->is_array() && windIt->size() == 3) {
        state.windDirection = glm::vec3((*windIt)[0].get<float>(),
                                        (*windIt)[1].get<float>(),
                                        (*windIt)[2].get<float>());
    }
    state.windSpeed = data.value("windSpeed", state.windSpeed);
    state.surfaceWetness = data.value("surfaceWetness", state.surfaceWetness);
    state.puddleAmount = data.value("puddleAmount", state.puddleAmount);
    state.surfaceDarkening = data.value("surfaceDarkening", state.surfaceDarkening);
    if (auto tintIt = data.find("surfaceTint"); tintIt != data.end() && tintIt->is_array() && tintIt->size() == 3) {
        state.surfaceTint = glm::vec3((*tintIt)[0].get<float>(),
                                      (*tintIt)[1].get<float>(),
                                      (*tintIt)[2].get<float>());
    }
    return state;
}

WeatherForecast Game::ParseForecast(const nlohmann::json& data) const {
    WeatherForecast forecast{};
    if (auto forecastIt = data.find("forecast"); forecastIt != data.end() && forecastIt->is_array()) {
        for (const auto& entry : *forecastIt) {
            if (!entry.is_object()) {
                continue;
            }
            WeatherForecastEntry slot{};
            slot.profile = entry.value("profile", std::string{});
            if (slot.profile.empty()) {
                continue;
            }
            slot.startHour = std::clamp(entry.value("startHour", slot.startHour), 0.0f, 24.0f);
            slot.durationHours = std::max(0.0f, entry.value("durationHours", slot.durationHours));
            slot.description = entry.value("description", slot.description);
            forecast.entries.push_back(std::move(slot));
        }
    }
    return forecast;
}

const WeatherProfile& Game::ResolveWeatherProfile(const std::string& name) const {
    auto it = m_weatherProfiles.find(name);
    if (it != m_weatherProfiles.end()) {
        return it->second;
    }
    auto defaultIt = m_weatherProfiles.find("default");
    if (defaultIt != m_weatherProfiles.end()) {
        return defaultIt->second;
    }
    static WeatherProfile fallback{};
    return fallback;
}

void Game::BroadcastWeatherEvent() {
    m_weatherEventPayload.state = m_weatherState;
    gm::core::Event::TriggerWithData(gotmilked::GameEvents::WeatherStateChanged, &m_weatherEventPayload);

    const bool profileChanged = (m_weatherState.activeProfile != m_lastCaptureWeatherProfile);
    const bool wetnessShift = std::abs(m_weatherState.surfaceWetness - m_lastCaptureWetness) > 0.15f;
    if (profileChanged || wetnessShift) {
        RequestEnvironmentCapture(EnvironmentCaptureFlags::Reflection | EnvironmentCaptureFlags::LightProbe);
    }
}

void Game::SyncWeatherService() {
    if (!m_weatherService) {
        return;
    }
    const float normalized = m_timeOfDayController.GetNormalizedTime();
    const float dayLength = std::max(1.0f, m_timeOfDayController.GetConfig().dayLengthSeconds);
    m_weatherService->SetCurrentWeather(m_weatherState);
    m_weatherService->SetTimeOfDay(normalized, dayLength, m_sunMoonState);
}

nlohmann::json Game::BuildNarrativeSaveState() const {
    auto buildArray = [](const std::unordered_set<std::string>& source) {
        std::vector<std::string> sorted(source.begin(), source.end());
        std::sort(sorted.begin(), sorted.end());
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& id : sorted) {
            arr.push_back(id);
        }
        return arr;
    };

    nlohmann::json narrative = nlohmann::json::object();
    narrative["completedQuests"] = buildArray(m_completedQuests);
    narrative["completedDialogues"] = buildArray(m_completedDialogues);
    return narrative;
}

void Game::RestoreNarrativeState(const nlohmann::json& saveJson) {
    m_completedQuests.clear();
    m_completedDialogues.clear();
    if (m_narrativeLog) {
        m_narrativeLog->Clear();
    }

    if (!saveJson.contains("narrative") || !saveJson["narrative"].is_object()) {
        return;
    }

    const auto& narrative = saveJson["narrative"];
    auto loadSet = [](const nlohmann::json& arr, std::unordered_set<std::string>& dest) {
        if (!arr.is_array()) {
            return;
        }
        for (const auto& value : arr) {
            if (value.is_string()) {
                dest.insert(value.get<std::string>());
            }
        }
    };

    if (auto it = narrative.find("completedQuests"); it != narrative.end()) {
        loadSet(*it, m_completedQuests);
    }
    if (auto it = narrative.find("completedDialogues"); it != narrative.end()) {
        loadSet(*it, m_completedDialogues);
    }
}

void Game::UpdateWeather(float dt) {
    const WeatherProfile& targetProfile = ResolveWeatherProfile(m_weatherState.activeProfile);
    auto damp = [dt](float current, float target, float rate) {
        float t = 1.0f - std::exp(-rate * dt);
        return glm::mix(current, target, t);
    };

    m_weatherState.surfaceWetness = damp(m_weatherState.surfaceWetness, targetProfile.surfaceWetness, 2.5f);
    m_weatherState.puddleAmount = damp(m_weatherState.puddleAmount, targetProfile.puddleAmount, 2.0f);
    m_weatherState.surfaceDarkening = damp(m_weatherState.surfaceDarkening, targetProfile.surfaceDarkening, 1.8f);
    float tintBlend = 1.0f - std::exp(-dt * 1.5f);
    m_weatherState.surfaceTint = glm::mix(m_weatherState.surfaceTint, targetProfile.surfaceTint, tintBlend);

    m_weatherClock += dt;
    const float gust = std::sin(m_weatherClock * 0.31f) * 0.25f;
    const float sway = std::cos(m_weatherClock * 0.17f) * 0.2f;
    glm::vec3 perturbation = glm::vec3(gust, 0.0f, sway);
    glm::vec3 desired = glm::normalize(m_weatherState.windDirection + perturbation);
    m_weatherState.windDirection = glm::normalize(glm::mix(m_weatherState.windDirection, desired, dt * 0.5f));

    BroadcastWeatherEvent();
}

void Game::UpdateWeatherAccumulation(float dt) {
    const float seconds = dt;
    if (seconds <= 0.0f) {
        return;
    }

    // Sunlight accumulation based on sun elevation above the horizon.
    const float sunElevationDeg = m_sunMoonState.sunElevationDeg;
    if (sunElevationDeg > 0.0f) {
        const float daylightFactor = std::clamp(sunElevationDeg / 90.0f, 0.0f, 1.0f);
        m_accumulatedSunlightHours += (seconds / 3600.0f) * daylightFactor;
    }

    // Rainfall accumulation keyed off the active profile and surface wetness.
    const WeatherProfile& activeProfile = ResolveWeatherProfile(m_weatherState.activeProfile);
    const bool isRainProfile = activeProfile.surfaceWetness > 0.3f || activeProfile.spawnMultiplier > 0.4f;
    if (isRainProfile) {
        const float rainfallRate = std::max(0.0f, activeProfile.surfaceWetness) * 5.0f; // mm/hour approximation
        m_accumulatedRainfallMm += (seconds / 3600.0f) * rainfallRate;
    }

    // Wetness accumulation averages recent surface wetness.
    const float wetnessRate = m_weatherState.surfaceWetness;
    m_accumulatedWetness += (seconds / 3600.0f) * wetnessRate;

    m_lastAmbientTemperatureC = ComputeAmbientTemperatureC();
    m_lastPrecipitationRate = isRainProfile ? std::max(0.0f, activeProfile.surfaceWetness) * 5.0f : 0.0f;

    if (m_weatherService) {
        WeatherService::EnvironmentSnapshot env{};
        env.ambientTemperatureC = m_lastAmbientTemperatureC;
        env.precipitationRate = m_lastPrecipitationRate;
        env.surfaceWetness = m_weatherState.surfaceWetness;
        m_weatherService->SetEnvironment(env);
    }
}

void Game::ResetWeatherAccumulation() {
    m_accumulatedSunlightHours = 0.0f;
    m_accumulatedRainfallMm = 0.0f;
    m_accumulatedWetness = 0.0f;
}

float Game::ComputeAmbientTemperatureC() const {
    // Simple model: base temperature from day/night cycle plus profile tint.
    const float baseTemp = 10.0f + 15.0f * m_sunMoonState.sunIntensity - 5.0f * m_weatherState.surfaceDarkening;
    const auto& profile = ResolveWeatherProfile(m_weatherState.activeProfile);
    const float tintInfluence = (profile.tint.r + profile.tint.g + profile.tint.b) / 3.0f;
    return baseTemp + (tintInfluence - 1.0f) * 10.0f;
}

float Game::ComputePrecipitationRate() const {
    const WeatherProfile& profile = ResolveWeatherProfile(m_weatherState.activeProfile);
    return std::max(0.0f, profile.surfaceWetness) * 5.0f;
}

void Game::SetCelestialConfig(const gm::scene::CelestialConfig& config) {
    m_timeOfDayController.SetConfig(config);
    m_sunMoonState = m_timeOfDayController.Evaluate();
    m_exposureAccumulator = config.exposureBias;
    m_sunMoonState.exposureCompensation = m_exposureAccumulator;
    UpdateExposure(0.0f);
    UpdateCelestialLights();
}

void Game::SetTimeOfDayNormalized(float normalized) {
    const auto& config = m_timeOfDayController.GetConfig();
    const float dayLength = std::max(1.0f, config.dayLengthSeconds);
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    m_timeOfDayController.SetTimeSeconds(clamped * dayLength);
    m_sunMoonState = m_timeOfDayController.Evaluate();
    UpdateExposure(0.0f);
    UpdateCelestialLights();
    SyncWeatherService();
}

void Game::SetWeatherProfile(const std::string& profileName) {
    if (!m_weatherProfiles.contains(profileName)) {
        gm::core::Logger::Warning("[Game] Weather profile '{}' not found", profileName);
        return;
    }
    if (m_weatherState.activeProfile == profileName) {
        return;
    }
    m_weatherState.activeProfile = profileName;
    gm::core::Logger::Info("[Game] Weather profile set to '{}'", profileName);
    BroadcastWeatherEvent();
    SyncWeatherService();
}

void Game::UpdateShadowCascades(const gm::rendering::CascadeShadowMap& cascades) {
    const auto& matrices = cascades.CascadeMatrices();
    const auto& splits = cascades.CascadeSplits();
    const int cascadeCount = std::min<int>(static_cast<int>(matrices.size()), 4);
    m_sunMoonState.sunCascadeCount = cascadeCount;
    for (int i = 0; i < cascadeCount; ++i) {
        m_sunMoonState.sunCascadeMatrices[i] = matrices[i];
        float split = (i < static_cast<int>(splits.size())) ? splits[i] : 1.0f;
        split = std::clamp(split + m_shadowCascadeBias, 0.0f, 1.0f);
        m_sunMoonState.sunCascadeSplits[i] = split;
    }
    for (int i = cascadeCount; i < 4; ++i) {
        m_sunMoonState.sunCascadeMatrices[i] = glm::mat4(1.0f);
        m_sunMoonState.sunCascadeSplits[i] = 1.0f;
    }
    if (cascadeCount > 0) {
        m_sunMoonState.sunViewProjection = matrices[0];
    } else {
        m_sunMoonState.sunViewProjection = glm::mat4(1.0f);
    }
}

void Game::UpdateShadowCascadeBias() {
    const float elevation = glm::clamp(m_sunMoonState.sunElevationDeg / 90.0f, -1.0f, 1.0f);
    if (elevation < 0.0f) {
        m_shadowCascadeBias = (elevation * 0.15f); // widen cascades at night
    } else {
        m_shadowCascadeBias = -(0.05f * (1.0f - elevation)); // compress cascades at noon
    }
}

void Game::RequestEnvironmentCapture(EnvironmentCaptureFlags flags) {
    if (flags == EnvironmentCaptureFlags::None) {
        return;
    }
    if (m_timeSinceLastEnvironmentCapture < m_environmentCaptureCooldown) {
        return;
    }
    m_pendingEnvironmentCaptures = m_pendingEnvironmentCaptures | flags;
}

EnvironmentCaptureFlags Game::ConsumeEnvironmentCaptureFlags() {
    EnvironmentCaptureFlags flags = m_pendingEnvironmentCaptures;
    m_pendingEnvironmentCaptures = EnvironmentCaptureFlags::None;
    return flags;
}

void Game::NotifyEnvironmentCapturePerformed(EnvironmentCaptureFlags /*flags*/) {
    m_timeSinceLastEnvironmentCapture = 0.0f;
    m_lastCaptureWeatherProfile = m_weatherState.activeProfile;
    m_lastCaptureWetness = m_weatherState.surfaceWetness;
    m_lastCaptureSunElevation = m_sunMoonState.sunElevationDeg;
}

void Game::ApplyProfilingPreset(ProfilingPreset preset) {
    struct PresetConfig {
        float timeOfDay = 0.5f;
        const char* weatherProfile = "default";
        float wetness = 0.0f;
        float puddles = 0.0f;
        float darkening = 0.0f;
        float windSpeed = 4.0f;
        glm::vec3 windDirection = glm::vec3(0.2f, 0.0f, 0.8f);
        const char* description = "";
    };

    PresetConfig config{};
    switch (preset) {
    case ProfilingPreset::SunnyMidday:
        config = {0.5f, "default", 0.05f, 0.0f, 0.05f, 4.0f, glm::vec3(0.2f, 0.0f, 0.8f), "Sunny midday"};
        break;
    case ProfilingPreset::StormyMidday:
        config = {0.58f, "heavy_rain", 0.85f, 0.6f, 0.45f, 12.0f, glm::vec3(0.1f, 0.0f, -0.9f), "Stormy"};
        break;
    case ProfilingPreset::DuskClear:
        config = {0.82f, "default", 0.1f, 0.05f, 0.2f, 3.0f, glm::vec3(-0.3f, 0.0f, 0.7f), "Dusk"};
        break;
    }

    SetTimeOfDayNormalized(config.timeOfDay);

    std::string profile = config.weatherProfile;
    if (!m_weatherProfiles.contains(profile)) {
        profile = "default";
    }
    SetWeatherProfile(profile);

    m_weatherState.surfaceWetness = config.wetness;
    m_weatherState.puddleAmount = config.puddles;
    m_weatherState.surfaceDarkening = config.darkening;
    m_weatherState.windSpeed = config.windSpeed;
    m_weatherState.windDirection = glm::normalize(config.windDirection);
    BroadcastWeatherEvent();
    SyncWeatherService();
    RequestEnvironmentCapture(EnvironmentCaptureFlags::Reflection | EnvironmentCaptureFlags::LightProbe);

    gm::core::Logger::Info("[Game] Applied profiling preset '{}'", config.description);
    if (auto* tooling = GetToolingFacade()) {
        tooling->AddNotification(fmt::format("Profiling preset: {}", config.description));
    }
}

bool Game::ApplyProfilingPreset(std::string_view presetName) {
    if (presetName == "sunny" || presetName == "Sunny" || presetName == "Sunny Midday") {
        ApplyProfilingPreset(ProfilingPreset::SunnyMidday);
        return true;
    }
    if (presetName == "stormy" || presetName == "Stormy" || presetName == "Stormy Midday") {
        ApplyProfilingPreset(ProfilingPreset::StormyMidday);
        return true;
    }
    if (presetName == "dusk" || presetName == "Dusk" || presetName == "Dusk Clear") {
        ApplyProfilingPreset(ProfilingPreset::DuskClear);
        return true;
    }
    return false;
}

void Game::SetupInput() {
    auto& inputManager = gm::core::InputManager::Instance();
    gm::core::InputBindings::SetupDefaultBindings(inputManager);
}

void Game::SetupGameplay() {
    if (!m_cameraRigSystem) {
        m_cameraRigSystem = std::make_shared<gm::gameplay::CameraRigSystem>();
    }
    m_cameraRigSystem->SetActiveCamera(m_camera.get());
    m_cameraRigSystem->SetWindow(m_window);
    m_cameraRigSystem->SetSceneContext(m_gameScene);

#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetCamera(GetRenderCamera());
        m_terrainEditingSystem->SetWindow(m_window);
        m_terrainEditingSystem->SetFovProvider([this]() -> float {
            return m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees()
                                     : gotmilked::GameConstants::Camera::DefaultFovDegrees;
        });
        m_terrainEditingSystem->SetSceneContext(m_gameScene);
    }
#endif

    auto extractOwnerInfo = [](const gm::Component& component) -> std::pair<std::string, glm::vec3> {
        std::string name;
        glm::vec3 position{0.0f};
        if (auto* owner = component.GetOwner()) {
            name = owner->GetName();
            if (auto transform = owner->GetTransform()) {
                position = transform->GetPosition();
            }
        }
        return {name, position};
    };

    auto playerPositionProvider = [this]() -> glm::vec3 {
        return m_camera ? m_camera->Position() : glm::vec3(0.0f);
    };

    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
        m_questSystem->SetPlayerPositionProvider(playerPositionProvider);
        m_questSystem->SetTriggerCallback([this, extractOwnerInfo](const gm::gameplay::QuestTriggerComponent& trigger,
                                                                  const gm::gameplay::QuestTriggerSystem::TriggerContext& ctx) {
            const std::string questId = trigger.GetQuestId();
            if (questId.empty()) {
                return;
            }
            const bool firstTrigger = m_completedQuests.insert(questId).second;
            const std::string message = firstTrigger
                ? fmt::format("Quest triggered: {}", questId)
                : fmt::format("Quest updated: {}", questId);
            gm::core::Logger::Info("[Game] {}", message);
            if (m_toolingFacade) {
                m_toolingFacade->AddNotification(message);
            }

            auto [objectName, worldPos] = extractOwnerInfo(trigger);
            if (m_scriptingHooks) {
                ScriptingHooks::QuestEvent evt;
                evt.questId = questId;
                evt.triggerObject = objectName;
                evt.location = worldPos;
                evt.repeatable = trigger.IsRepeatable();
                evt.triggeredFromSceneLoad = ctx.source == gm::gameplay::QuestTriggerSystem::TriggerSource::SceneLoad;
                m_scriptingHooks->DispatchQuestEvent(evt);
            }

            gotmilked::QuestTriggerEventPayload payload;
            payload.questId = questId;
            payload.triggerObject = objectName;
            payload.location = worldPos;
            payload.sceneLoad = ctx.source == gm::gameplay::QuestTriggerSystem::TriggerSource::SceneLoad;
            gm::core::Event::TriggerWithData(gotmilked::GameEvents::QuestTriggered, &payload);
        });
    }

    if (m_dialogueSystem) {
        m_dialogueSystem->SetSceneContext(m_gameScene);
        m_dialogueSystem->SetPlayerPositionProvider(playerPositionProvider);
        m_dialogueSystem->SetTriggerCallback([this, extractOwnerInfo](const gm::gameplay::DialogueTriggerComponent& trigger,
                                                                      const gm::gameplay::DialogueTriggerSystem::TriggerContext& ctx) {
            const std::string dialogueId = trigger.GetDialogueId();
            if (dialogueId.empty()) {
                return;
            }

            auto [objectName, worldPos] = extractOwnerInfo(trigger);
            const bool firstDialogue = m_completedDialogues.insert(dialogueId).second;
            std::string message = fmt::format("Dialogue triggered: {}", dialogueId);
            if (!objectName.empty()) {
                message += fmt::format(" ({})", objectName);
            }
            if (firstDialogue) {
                message += " [new]";
            }
            gm::core::Logger::Info("[Game] {}", message);
            if (m_toolingFacade) {
                m_toolingFacade->AddNotification(message);
            }

            if (m_scriptingHooks) {
                ScriptingHooks::DialogueEvent evt;
                evt.dialogueId = dialogueId;
                evt.speakerObject = objectName;
                evt.location = worldPos;
                evt.repeatable = trigger.IsRepeatable();
                evt.autoStart = trigger.AutoStart();
                evt.triggeredFromSceneLoad = ctx.source == gm::gameplay::DialogueTriggerSystem::TriggerSource::SceneLoad;
                m_scriptingHooks->DispatchDialogueEvent(evt);
            }

            gotmilked::DialogueTriggerEventPayload payload;
            payload.dialogueId = dialogueId;
            payload.triggerObject = objectName;
            payload.location = worldPos;
            payload.sceneLoad = ctx.source == gm::gameplay::DialogueTriggerSystem::TriggerSource::SceneLoad;
            payload.autoStart = trigger.AutoStart();
            gm::core::Event::TriggerWithData(gotmilked::GameEvents::DialogueTriggered, &payload);
        });
    }
}

void Game::SetupSaveSystem() {
    m_saveManager = std::make_unique<gm::save::SaveManager>(m_config.paths.saves);
}

void Game::CaptureCelestialLights() {
    if (!m_gameScene) {
        return;
    }

    if (auto sun = m_gameScene->FindGameObjectByName("Sun")) {
        m_sunLight = sun->GetComponent<gm::LightComponent>();
    }
    if (auto moon = m_gameScene->FindGameObjectByName("Moon")) {
        m_moonLight = moon->GetComponent<gm::LightComponent>();
    }
}

void Game::UpdateCelestialLights() {
    auto applyDirectional = [](const std::shared_ptr<gm::LightComponent>& light,
                               const glm::vec3& direction,
                               float intensity,
                               const glm::vec3& color) {
        if (!light) {
            return;
        }
        glm::vec3 safeDir = direction;
        if (glm::dot(safeDir, safeDir) < 1e-4f) {
            safeDir = glm::vec3(0.0f, -1.0f, 0.0f);
        }
        light->SetType(gm::LightComponent::LightType::Directional);
        light->SetDirection(glm::normalize(-safeDir));
        light->SetIntensity(intensity);
        light->SetColor(color);
    };

    const float sunScalar = glm::clamp(m_sunMoonState.sunIntensity, 0.0f, 1.0f);
    const float sunElevationFactor = glm::clamp((m_sunMoonState.sunElevationDeg + 10.0f) / 80.0f, 0.0f, 1.0f);
    const glm::vec3 sunriseColor(1.0f, 0.72f, 0.45f);
    const glm::vec3 noonColor(1.0f, 0.97f, 0.92f);
    const glm::vec3 sunColor = glm::mix(sunriseColor, noonColor, sunElevationFactor);
    const float sunIntensity = glm::mix(0.05f, gotmilked::GameConstants::Light::SunIntensity, sunScalar);
    applyDirectional(m_sunLight.lock(), m_sunMoonState.sunDirection, sunIntensity, sunColor);

    const float moonScalar = glm::clamp(m_sunMoonState.moonIntensity, 0.0f, 1.0f);
    const float moonIntensity = gotmilked::GameConstants::Light::MoonIntensity * moonScalar;
    const glm::vec3 moonColor = gotmilked::GameConstants::Light::MoonColor;
    applyDirectional(m_moonLight.lock(), m_sunMoonState.moonDirection, moonIntensity, moonColor);

    if (m_gameScene) {
        m_gameScene->SetCelestialLighting(m_sunMoonState, sunColor, sunIntensity, moonColor, moonIntensity);
    }
}

void Game::UpdateExposure(float /*dt*/) {
    const auto& config = m_timeOfDayController.GetConfig();
    const float referenceLux = std::max(0.1f, config.exposureReferenceLux);
    const float lux = std::max(0.1f, m_sunMoonState.sunIlluminanceLux);
    const float ev = std::log2(lux / referenceLux);
    const float targetEv = config.exposureTargetEv;
    const float rawCompensation = std::pow(2.0f, targetEv - ev) * config.exposureBias;
    const float minComp = std::max(0.01f, config.exposureMin);
    const float maxComp = std::max(minComp, config.exposureMax);
    const float compensation = glm::clamp(rawCompensation, minComp, maxComp);
    const float smoothing = glm::clamp(config.exposureSmoothing, 0.0f, 0.999f);
    m_exposureAccumulator = glm::mix(compensation, m_exposureAccumulator, smoothing);
    m_sunMoonState.exposureCompensation = m_exposureAccumulator;
    UpdateShadowCascadeBias();

    if (std::abs(m_sunMoonState.sunElevationDeg - m_lastCaptureSunElevation) > 10.0f) {
        RequestEnvironmentCapture(EnvironmentCaptureFlags::LightProbe);
    }
}


namespace {
constexpr const char* kStarterSceneFilename = "starter.scene.json";
}

void Game::SetupScene() {
    if (!m_sceneManager) {
        gm::core::Logger::Error("[Game] No SceneManager provided");
        return;
    }

    m_gameScene = m_sceneManager->LoadScene("GameScene");
    if (!m_gameScene) {
        gm::core::Logger::Error("[Game] Failed to create game scene");
        return;
    }

    if (m_cameraRigSystem) {
        m_cameraRigSystem->SetSceneContext(m_gameScene);
        m_gameScene->RegisterSystem(m_cameraRigSystem);
    }
#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetSceneContext(m_gameScene);
        m_gameScene->RegisterSystem(m_terrainEditingSystem);
    }
#endif
    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
        m_gameScene->RegisterSystem(m_questSystem);
    }
    if (m_dialogueSystem) {
        m_dialogueSystem->SetSceneContext(m_gameScene);
        m_gameScene->RegisterSystem(m_dialogueSystem);
    }

    EnsureCameraRig();

    std::filesystem::path starterRoot = m_config.paths.saves;
    if (starterRoot.empty()) {
        starterRoot = m_assetsDir / "saves";
    }
    std::error_code canonicalSavesEc;
    auto canonicalSaves = std::filesystem::weakly_canonical(starterRoot, canonicalSavesEc);
    if (canonicalSavesEc) {
        canonicalSaves = std::filesystem::absolute(starterRoot);
    }
    const auto starterScenePath = (canonicalSaves / kStarterSceneFilename).lexically_normal();

    bool loadedFromDisk = false;
    std::error_code existsEc;
    if (std::filesystem::exists(starterScenePath, existsEc)) {
        gm::core::Logger::Info("[Game] Loading starter scene from '{}'", starterScenePath.string());
        if (gm::SceneSerializer::LoadFromFile(*m_gameScene, starterScenePath.string())) {
            loadedFromDisk = true;
        } else {
            gm::core::Logger::Warning("[Game] Failed to load starter scene from '{}'; rebuilding default scene", starterScenePath.string());
            m_gameScene->Cleanup();
        }
    } else if (existsEc) {
        gm::core::Logger::Warning("[Game] Could not check starter scene at '{}': {}", starterScenePath.string(), existsEc.message());
    }

    m_gameScene->SetParallelGameObjectUpdates(true);

    if (loadedFromDisk && m_gameScene->GetAllGameObjects().empty()) {
        gm::core::Logger::Warning("[Game] Starter scene file '{}' was empty; rebuilding default scene", starterScenePath.string());
        loadedFromDisk = false;
    }

    if (!loadedFromDisk) {
        m_gameScene->Cleanup();

        auto fovProvider = [this]() -> float {
        return m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees() : 60.0f;
        };
        gotmilked::PopulateInitialScene(*m_gameScene, *m_camera, m_resources, m_window, fovProvider);

        std::error_code dirEc;
        std::filesystem::create_directories(canonicalSaves, dirEc);
        if (dirEc) {
            gm::core::Logger::Warning("[Game] Failed to create saves directory '{}': {}", canonicalSaves.string(), dirEc.message());
        } else if (gm::SceneSerializer::SaveToFile(*m_gameScene, starterScenePath.string())) {
            gm::core::Logger::Info("[Game] Generated starter scene at '{}'", starterScenePath.string());
        } else {
            gm::core::Logger::Warning("[Game] Failed to save generated starter scene to '{}'", starterScenePath.string());
        }
    } else {
        gm::core::Logger::Info("[Game] Starter scene loaded successfully");
    }

    gm::core::Logger::Info("[Game] Game scene initialized successfully");

    ApplyResourcesToScene();

    CaptureCelestialLights();
    UpdateCelestialLights();

    if (m_cameraRigSystem) {
        m_cameraRigSystem->SetSceneContext(m_gameScene);
    }
}

void Game::Update(float dt) {
    gm::utils::Profiler::Instance().BeginFrame();
    m_lastDeltaTime = dt;
    m_timeSinceLastEnvironmentCapture += dt;
    m_timeOfDayController.Advance(dt);
    m_sunMoonState = m_timeOfDayController.Evaluate();
    UpdateExposure(dt);
    UpdateCelestialLights();
    UpdateWeather(dt);
    UpdateWeatherAccumulation(dt);
    SyncWeatherService();
    if (m_loopController) {
        m_loopController->Update(dt);
    }
}


void Game::Render() {
    if (m_renderer) {
        m_renderer->Render();
    }
    gm::utils::Profiler::Instance().EndFrame();
}

void Game::Shutdown() {
    SetDebugViewportCameraActive(false);
    if (m_shutdownController) {
        m_shutdownController->Shutdown();
    }
}

void Game::SetupEventSubscriptions() {
    if (!m_eventRouter) {
        m_eventRouter = std::make_unique<EventRouter>();
    }
    m_eventRouter->Clear();

    auto notify = [this](const char* message) {
        if (m_toolingFacade) {
            m_toolingFacade->AddNotification(message);
        }
    };

    auto refreshHud = [this]() {
        if (m_toolingFacade) {
            m_toolingFacade->RefreshHud();
        }
    };

    const struct {
        const char* name;
        gm::core::Event::EventCallback callback;
    } handlers[] = {
        {gotmilked::GameEvents::ResourceShaderLoaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Shader loaded");
             notify("Shader loaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceShaderReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Shader reloaded");
             notify("Shader reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceTextureLoaded, [this, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Texture loaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceTextureReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Texture reloaded");
             notify("Texture reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceMeshLoaded, [this, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Mesh loaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceMeshReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Debug("[Game] Event: Mesh reloaded");
             notify("Mesh reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceAllReloaded, [this, notify, refreshHud]() {
             gm::core::Logger::Info("[Game] Event: All resources reloaded");
             notify("All resources reloaded");
             refreshHud();
         }},
        {gotmilked::GameEvents::ResourceLoadFailed, [this, notify]() {
             gm::core::Logger::Warning("[Game] Event: Resource load failed");
             notify("Resource load failed");
         }},
        {gotmilked::GameEvents::SceneQuickSaved, [this]() {
             gm::core::Logger::Debug("[Game] Event: Scene quick saved");
         }},
        {gotmilked::GameEvents::SceneQuickLoaded, [this]() {
             gm::core::Logger::Debug("[Game] Event: Scene quick loaded");
         }},
        {gotmilked::GameEvents::GameInitialized, [this]() {
             gm::core::Logger::Info("[Game] Event: Game initialized");
         }},
        {gotmilked::GameEvents::GameShutdown, [this]() {
             gm::core::Logger::Info("[Game] Event: Game shutdown");
         }},
    };

    for (const auto& handler : handlers) {
        m_eventRouter->Register(handler.name, handler.callback);
    }
}

void Game::SetupResourceHotReload() {
    m_hotReloader.SetEnabled(m_config.hotReload.enable);
    m_hotReloader.SetPollInterval(m_config.hotReload.pollIntervalSeconds);

    if (!m_config.hotReload.enable) {
        return;
    }

    if (!m_resources.GetShaderVertPath().empty() && !m_resources.GetShaderFragPath().empty()) {
        m_hotReloader.AddWatch(
            "game_shader",
            {std::filesystem::path(m_resources.GetShaderVertPath()), std::filesystem::path(m_resources.GetShaderFragPath())},
            [this]() {
                gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadShaderDetected);
                const std::string shaderGuid = m_resources.GetShaderGuid();
                bool ok = m_resources.ReloadShader(shaderGuid);
                if (ok) {
                    if (m_sceneResources) {
                        m_sceneResources->RefreshShaders({shaderGuid});
                    }
                    gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadShaderReloaded);
                }
                return ok;
            });
    }

    if (!m_resources.GetTexturePath().empty()) {
        m_hotReloader.AddWatch(
            "game_texture",
            {std::filesystem::path(m_resources.GetTexturePath())},
            [this]() {
                gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadTextureDetected);
                bool ok = m_resources.ReloadTexture();
                if (ok) {
                    ApplyResourcesToScene();
                    gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadTextureReloaded);
                }
                return ok;
            });
    }

    if (!m_resources.GetMeshPath().empty()) {
        m_hotReloader.AddWatch(
            "game_mesh",
            {std::filesystem::path(m_resources.GetMeshPath())},
            [this]() {
                gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadMeshDetected);
                const std::string meshGuid = m_resources.GetMeshPath().empty() ? std::string() : m_resources.GetMeshGuid();
                bool ok = meshGuid.empty() ? m_resources.ReloadMesh() : m_resources.ReloadMesh(meshGuid);
                if (ok) {
                    if (m_sceneResources) {
                        if (!meshGuid.empty()) {
                            m_sceneResources->RefreshMeshes({meshGuid});
                        } else {
                            m_sceneResources->ApplyResourcesToStaticMeshComponents();
                        }
                    }
                    gm::core::Event::Trigger(gotmilked::GameEvents::HotReloadMeshReloaded);
                }
                return ok;
            });
    }

    m_hotReloader.ForcePoll();
}

void Game::ApplyResourcesToScene() {
    if (m_sceneResources) {
        m_sceneResources->ApplyResourcesToScene();
    }
    EnsureCameraRig();
    if (m_tooling) {
        m_tooling->SetCamera(GetRenderCamera());
    }
    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
    }
#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetCamera(GetRenderCamera());
        m_terrainEditingSystem->RefreshBindings();
    }
#endif
}

void Game::ApplyResourcesToStaticMeshComponents() {
    if (m_sceneResources) {
        m_sceneResources->ApplyResourcesToStaticMeshComponents();
    }
}

#if GM_DEBUG_TOOLS
void Game::ApplyResourcesToTerrain() {
    if (m_sceneResources) {
        m_sceneResources->ApplyResourcesToTerrain();
    }
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->RefreshBindings();
    }
}

#endif

void Game::EnsureCameraRig() {
    if (!m_gameScene) {
        return;
    }

    std::shared_ptr<gm::GameObject> cameraRigObject;
    for (const auto& obj : m_gameScene->GetAllGameObjects()) {
        if (!obj) {
            continue;
        }
        if (obj->GetComponent<gm::gameplay::CameraRigComponent>()) {
            // A valid camera rig already exists
            return;
        }

        if (!cameraRigObject && obj->GetName() == "CameraRig") {
            cameraRigObject = obj;
        }
    }

    bool spawnedNewObject = false;
    if (!cameraRigObject) {
        cameraRigObject = m_gameScene->SpawnGameObject("CameraRig");
        if (!cameraRigObject) {
            gm::core::Logger::Warning("[Game] Failed to spawn CameraRig GameObject");
            return;
        }
        spawnedNewObject = true;
    }

    auto rig = cameraRigObject->GetComponent<gm::gameplay::CameraRigComponent>();
    if (!rig) {
        rig = cameraRigObject->AddComponent<gm::gameplay::CameraRigComponent>();
        if (!rig) {
            gm::core::Logger::Warning("[Game] Failed to add CameraRigComponent to CameraRig GameObject");
            return;
        }
        rig->SetRigId("PrimaryCamera");
        rig->SetInitialFov(gotmilked::GameConstants::Camera::DefaultFovDegrees);
    } else if (spawnedNewObject) {
        // Newly spawned object already had a rig component (unlikely), but ensure defaults
        rig->SetRigId("PrimaryCamera");
        rig->SetInitialFov(gotmilked::GameConstants::Camera::DefaultFovDegrees);
    }
}

gm::Camera* Game::GetRenderCamera() const {
#if GM_DEBUG_TOOLS
    if (m_viewportCameraActive && m_viewportCamera) {
        return m_viewportCamera.get();
    }
#endif
    return m_camera.get();
}

float Game::GetRenderCameraFov() const {
#if GM_DEBUG_TOOLS
    if (m_viewportCameraActive && m_viewportCameraController) {
        return m_viewportCameraController->GetFovDegrees();
    }
#endif
    return m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees()
                             : gotmilked::GameConstants::Camera::DefaultFovDegrees;
}

void Game::SetDebugViewportCameraActive(bool enabled) {
#if GM_DEBUG_TOOLS
    if (enabled == m_viewportCameraActive) {
        return;
    }

    if (enabled) {
        if (!m_viewportCamera) {
            m_viewportCamera = std::make_unique<gm::Camera>();
        }
        if (m_camera) {
            m_viewportSavedPosition = m_camera->Position();
            m_viewportSavedForward = m_camera->Front();
            m_viewportSavedFov = m_cameraRigSystem ? m_cameraRigSystem->GetFovDegrees()
                                                   : gotmilked::GameConstants::Camera::DefaultFovDegrees;
            m_viewportCameraHasSavedPose = true;
        }
        m_viewportCamera->SetPosition(m_viewportSavedPosition);
        m_viewportCamera->SetForward(m_viewportSavedForward);
        m_viewportCamera->SetFov(m_viewportSavedFov);

        gm::gameplay::FlyCameraController::Config config;
        config.initialFov = m_viewportSavedFov;
        config.fovMin = 30.0f;
        config.fovMax = 100.0f;
        config.fovScrollSensitivity = 2.0f;
        m_viewportCameraController = std::make_unique<gm::gameplay::FlyCameraController>(
            *m_viewportCamera,
            m_window,
            config);
        if (m_gameScene) {
            m_viewportCameraController->SetScene(m_gameScene);
        }
        m_viewportCameraActive = true;

        if (m_tooling) {
            m_tooling->SetCamera(m_viewportCamera.get());
        }
        if (m_terrainEditingSystem) {
            m_terrainEditingSystem->SetCamera(m_viewportCamera.get());
        }
    } else {
        if (m_viewportCameraController && m_viewportCamera) {
            m_viewportSavedPosition = m_viewportCamera->Position();
            m_viewportSavedForward = m_viewportCamera->Front();
            m_viewportSavedFov = m_viewportCameraController->GetFovDegrees();
        }
        if (m_tooling) {
            m_tooling->SetCamera(m_camera.get());
        }
        if (m_terrainEditingSystem) {
            m_terrainEditingSystem->SetCamera(m_camera.get());
        }
        m_viewportCameraController.reset();
        m_viewportCamera.reset();
        m_viewportCameraHasSavedPose = false;
        m_viewportCameraActive = false;
    }
#else
    (void)enabled;
#endif
}

bool Game::IsDebugViewportCameraActive() const {
#if GM_DEBUG_TOOLS
    return m_viewportCameraActive;
#else
    return false;
#endif
}

void Game::UpdateViewportCamera(float deltaTime, bool inputSuppressed) {
#if GM_DEBUG_TOOLS
    if (!m_viewportCameraActive || !m_viewportCameraController) {
        return;
    }
    m_viewportCameraController->SetWindow(m_window);
    if (m_gameScene) {
        m_viewportCameraController->SetScene(m_gameScene);
    }
    m_viewportCameraController->SetInputSuppressed(inputSuppressed);
    m_viewportCameraController->Update(deltaTime);
    if (m_viewportCamera) {
        m_viewportSavedPosition = m_viewportCamera->Position();
        m_viewportSavedForward = m_viewportCamera->Front();
        m_viewportSavedFov = m_viewportCameraController->GetFovDegrees();
        m_viewportCameraHasSavedPose = true;
    }
#else
    (void)deltaTime;
    (void)inputSuppressed;
#endif
}

#if GM_DEBUG_TOOLS
void Game::ApplyWeatherStateDebug(const WeatherState& state, bool broadcastEvent) {
    WeatherState applied = state;
    if (applied.activeProfile.empty() || !m_weatherProfiles.contains(applied.activeProfile)) {
        applied.activeProfile = m_weatherState.activeProfile.empty() ? "default" : m_weatherState.activeProfile;
    }
    const float windLength = glm::length(applied.windDirection);
    if (windLength < 0.001f) {
        applied.windDirection = glm::vec3(0.2f, 0.0f, 0.8f);
    } else {
        applied.windDirection /= windLength;
    }
    applied.windSpeed = std::max(0.0f, applied.windSpeed);
    applied.surfaceWetness = std::clamp(applied.surfaceWetness, 0.0f, 1.0f);
    applied.puddleAmount = std::clamp(applied.puddleAmount, 0.0f, 1.0f);
    applied.surfaceDarkening = std::clamp(applied.surfaceDarkening, 0.0f, 1.0f);

    m_weatherState = applied;
    if (broadcastEvent) {
        BroadcastWeatherEvent();
    } else {
        SyncWeatherService();
    }
}

void Game::OverrideWeatherForecastDebug(const WeatherForecast& forecast) {
    m_weatherForecast = forecast;
    SyncWeatherService();
}

void Game::TriggerWeatherStateEventDebug() {
    BroadcastWeatherEvent();
}

void Game::RequestEnvironmentCaptureDebug(EnvironmentCaptureFlags flags) {
    RequestEnvironmentCapture(flags);
}
#endif

void Game::PerformQuickSave() {
    if (!m_saveManager || !m_gameScene || !m_camera || !m_cameraRigSystem) {
        gm::core::Logger::Warning("[Game] QuickSave unavailable (missing dependencies)");
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickSave unavailable");
        return;
    }

    auto data = gm::save::SaveSnapshotHelpers::CaptureSnapshot(
        m_camera.get(),
        m_gameScene,
        [this]() { return m_cameraRigSystem ? m_cameraRigSystem->GetWorldTimeSeconds() : 0.0; });
    
    // Add FOV to save data
    data.cameraFov = m_cameraRigSystem->GetFovDegrees();

#if GM_DEBUG_TOOLS
    if (m_gameScene) {
        auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
        if (terrainObject) {
            if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                data.terrainResolution = terrain->GetResolution();
                data.terrainSize = terrain->GetTerrainSize();
                data.terrainMinHeight = terrain->GetMinHeight();
                data.terrainMaxHeight = terrain->GetMaxHeight();
                data.terrainHeights = terrain->GetHeights();
                data.terrainTextureTiling = terrain->GetTextureTiling();
                data.terrainBaseTextureGuid = terrain->GetBaseTextureGuid();
                data.terrainActivePaintLayer = terrain->GetActivePaintLayerIndex();
                data.terrainPaintLayers.clear();
                const int paintLayerCount = terrain->GetPaintLayerCount();
                data.terrainPaintLayers.reserve(paintLayerCount);
                for (int layer = 0; layer < paintLayerCount; ++layer) {
                    gm::save::SaveGameData::TerrainPaintLayerData layerData;
                    layerData.guid = terrain->GetPaintTextureGuid(layer);
                    layerData.enabled = terrain->IsPaintLayerEnabled(layer);
                    const auto& weights = terrain->GetPaintLayerWeights(layer);
                    layerData.weights.assign(weights.begin(), weights.end());
                    data.terrainPaintLayers.push_back(std::move(layerData));
                }
            }
        }
    }
#endif

    // Serialize the scene to include all GameObjects and their properties
    std::string sceneJsonString = gm::SceneSerializer::Serialize(*m_gameScene);
    nlohmann::json sceneJson = nlohmann::json::parse(sceneJsonString);
    
    // Merge SaveGameData into the scene JSON
    nlohmann::json saveJson = {
        {"version", gm::save::SaveVersionToJson(data.version)},
        {"sceneName", data.sceneName},
        {"camera", {
            {"position", {data.cameraPosition.x, data.cameraPosition.y, data.cameraPosition.z}},
            {"forward", {data.cameraForward.x, data.cameraForward.y, data.cameraForward.z}},
            {"fov", data.cameraFov}
        }},
        {"worldTime", data.worldTime}
    };
    
    if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
        nlohmann::json terrainJson = {
            {"resolution", data.terrainResolution},
            {"size", data.terrainSize},
            {"minHeight", data.terrainMinHeight},
            {"maxHeight", data.terrainMaxHeight},
            {"heights", data.terrainHeights},
            {"textureTiling", data.terrainTextureTiling},
            {"baseTextureGuid", data.terrainBaseTextureGuid},
            {"activePaintLayer", data.terrainActivePaintLayer}
        };

        nlohmann::json paintLayers = nlohmann::json::array();
        for (const auto& layer : data.terrainPaintLayers) {
            nlohmann::json layerJson;
            layerJson["guid"] = layer.guid;
            layerJson["enabled"] = layer.enabled;
            layerJson["weights"] = layer.weights;
            paintLayers.push_back(std::move(layerJson));
        }
        terrainJson["paintLayers"] = std::move(paintLayers);

        saveJson["terrain"] = std::move(terrainJson);
    }
    
    // Merge scene data with save data (scene data takes precedence for gameObjects)
    saveJson["gameObjects"] = sceneJson["gameObjects"];
    saveJson["name"] = sceneJson.value("name", data.sceneName);
    saveJson["isPaused"] = sceneJson.value("isPaused", false);

    nlohmann::json metadata;
    metadata["runtimeVersion"] = gm::save::SaveVersionToJson(gm::save::SaveVersion::Current());
    metadata["versionString"] = data.version.ToString();

    bool terrainFallbackApplied = false;
    if (m_saveManager) {
        nlohmann::json previousJson;
        auto previousResult = m_saveManager->LoadMostRecentQuickSaveJson(previousJson);
        if (previousResult.success) {
            if (!saveJson.contains("terrain")) {
                gm::save::MergeTerrainIfMissing(saveJson, previousJson);
                terrainFallbackApplied = saveJson.contains("terrain");
            }

            auto diffSummary = gm::save::ComputeSaveDiff(previousJson, saveJson);
            nlohmann::json diffJson;
            diffJson["versionChanged"] = diffSummary.versionChanged;
            diffJson["terrainChanged"] = diffSummary.terrainChanged;
            diffJson["questStateChanged"] = diffSummary.questStateChanged;
            diffJson["terrainFallbackApplied"] = terrainFallbackApplied;
            if (!diffSummary.terrainDiff.is_null()) {
                diffJson["terrainDiff"] = diffSummary.terrainDiff;
            }
            if (diffSummary.questStateChanged) {
                diffJson["questChanges"] = diffSummary.questChanges;
                for (const auto& change : diffSummary.questChanges) {
                    gm::core::Logger::Info("[Game] Quest diff: {}", change);
                }
            }
            if (diffSummary.dialogueStateChanged) {
                diffJson["dialogueChanges"] = diffSummary.dialogueChanges;
                for (const auto& change : diffSummary.dialogueChanges) {
                    gm::core::Logger::Info("[Game] Dialogue diff: {}", change);
                }
            }
            if (diffSummary.terrainChanged) {
                gm::core::Logger::Info("[Game] Terrain data changed since last quick save");
            }
            if (diffSummary.versionChanged) {
                gm::core::Logger::Info("[Game] Save version updated to {}", data.version.ToString());
            }
            metadata["diff"] = std::move(diffJson);
        } else if (previousResult.message != "No quick save found") {
            gm::core::Logger::Warning("[Game] Unable to load previous quick save for diff: {}", previousResult.message);
        }
    }

    if (terrainFallbackApplied) {
        gm::core::Logger::Info("[Game] Applied terrain data fallback from previous quick save");
    }

    saveJson["metadata"] = std::move(metadata);
    saveJson["narrative"] = BuildNarrativeSaveState();
    saveJson["weatherAccumulation"] = {
        {"sunlightHours", m_accumulatedSunlightHours},
        {"rainfallMm", m_accumulatedRainfallMm},
        {"wetness", m_accumulatedWetness},
        {"ambientTempC", m_lastAmbientTemperatureC},
        {"precipRate", m_lastPrecipitationRate}
    };
    saveJson["weather"] = {
        {"profile", m_weatherState.activeProfile},
        {"windDirection", {m_weatherState.windDirection.x, m_weatherState.windDirection.y, m_weatherState.windDirection.z}},
        {"windSpeed", m_weatherState.windSpeed},
        {"surfaceWetness", m_weatherState.surfaceWetness},
        {"puddleAmount", m_weatherState.puddleAmount},
        {"surfaceDarkening", m_weatherState.surfaceDarkening},
        {"surfaceTint", {m_weatherState.surfaceTint.x, m_weatherState.surfaceTint.y, m_weatherState.surfaceTint.z}}
    };
    if (const auto& entries = m_weatherForecast.entries; !entries.empty()) {
        nlohmann::json forecast = nlohmann::json::array();
        for (const auto& entry : entries) {
            forecast.push_back({
                {"profile", entry.profile},
                {"startHour", entry.startHour},
                {"durationHours", entry.durationHours},
                {"description", entry.description}
            });
        }
        saveJson["weatherForecast"] = std::move(forecast);
    }

    // Save using SaveManager but with the merged JSON
    auto result = m_saveManager->QuickSaveWithJson(saveJson);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickSave failed: {}", result.message);
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickSave failed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneSaveFailed);
    } else {
        gm::core::Logger::Info("[Game] QuickSave completed (with GameObjects)");
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickSave completed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickSaved);
    }
}

void Game::PerformQuickLoad() {
    if (!m_saveManager || !m_gameScene || !m_camera || !m_cameraRigSystem) {
        gm::core::Logger::Warning("[Game] QuickLoad unavailable (missing dependencies)");
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickLoad unavailable");
        return;
    }

    // Try loading with JSON first (new format with GameObjects)
    nlohmann::json saveJson;
    auto jsonResult = m_saveManager->QuickLoadWithJson(saveJson);
    
    if (jsonResult.success && saveJson.contains("gameObjects") && saveJson["gameObjects"].is_array()) {
        gm::save::SaveVersion fileVersion = gm::save::SaveVersion::Current();
        if (saveJson.contains("version")) {
            fileVersion = gm::save::ParseSaveVersion(saveJson["version"]);
        } else {
            gm::core::Logger::Warning("[Game] QuickLoad: save is missing version information; assuming current");
        }
        const auto runtimeVersion = gm::save::SaveVersion::Current();
        if (!fileVersion.IsCompatibleWith(runtimeVersion)) {
            gm::core::Logger::Warning(
                "[Game] QuickLoad: save version {} is not fully compatible with runtime {}; attempting migration",
                fileVersion.ToString(), runtimeVersion.ToString());
        }

        RestoreNarrativeState(saveJson);
        if (auto accumIt = saveJson.find("weatherAccumulation"); accumIt != saveJson.end() && accumIt->is_object()) {
            m_accumulatedSunlightHours = accumIt->value("sunlightHours", m_accumulatedSunlightHours);
            m_accumulatedRainfallMm = accumIt->value("rainfallMm", m_accumulatedRainfallMm);
            m_accumulatedWetness = accumIt->value("wetness", m_accumulatedWetness);
            m_lastAmbientTemperatureC = accumIt->value("ambientTempC", m_lastAmbientTemperatureC);
            m_lastPrecipitationRate = accumIt->value("precipRate", m_lastPrecipitationRate);
        }
        if (auto weatherIt = saveJson.find("weather"); weatherIt != saveJson.end() && weatherIt->is_object()) {
            m_weatherState.activeProfile = weatherIt->value("profile", m_weatherState.activeProfile);
            if (auto wind = weatherIt->find("windDirection"); wind != weatherIt->end() && wind->is_array() && wind->size() == 3) {
                m_weatherState.windDirection = glm::vec3((*wind)[0].get<float>(),
                                                         (*wind)[1].get<float>(),
                                                         (*wind)[2].get<float>());
            }
            m_weatherState.windSpeed = weatherIt->value("windSpeed", m_weatherState.windSpeed);
            m_weatherState.surfaceWetness = weatherIt->value("surfaceWetness", m_weatherState.surfaceWetness);
            m_weatherState.puddleAmount = weatherIt->value("puddleAmount", m_weatherState.puddleAmount);
            m_weatherState.surfaceDarkening = weatherIt->value("surfaceDarkening", m_weatherState.surfaceDarkening);
            if (auto tint = weatherIt->find("surfaceTint"); tint != weatherIt->end() && tint->is_array() && tint->size() == 3) {
                m_weatherState.surfaceTint = glm::vec3((*tint)[0].get<float>(),
                                                       (*tint)[1].get<float>(),
                                                       (*tint)[2].get<float>());
            }
        }
        if (auto forecastIt = saveJson.find("weatherForecast"); forecastIt != saveJson.end() && forecastIt->is_array()) {
            WeatherForecast restored;
            for (const auto& entry : *forecastIt) {
                if (!entry.is_object()) {
                    continue;
                }
                WeatherForecastEntry f;
                f.profile = entry.value("profile", std::string{});
                f.startHour = entry.value("startHour", f.startHour);
                f.durationHours = entry.value("durationHours", f.durationHours);
                f.description = entry.value("description", std::string{});
                restored.entries.push_back(std::move(f));
            }
            if (!restored.entries.empty()) {
                m_weatherForecast = restored;
                if (m_weatherService) {
                    m_weatherService->SetForecast(m_weatherForecast);
                }
            }
        }

        // New format with GameObjects - deserialize the scene
        std::string jsonString = saveJson.dump();
#if GM_DEBUG_TOOLS
        if (m_debugMenu) {
            m_debugMenu->BeginSceneReload();
        }
#endif
        bool ok = gm::SceneSerializer::Deserialize(*m_gameScene, jsonString);
        gm::core::Logger::Info("[Game] QuickLoad JSON deserialize result: {}", ok ? "success" : "failure");
#if GM_DEBUG_TOOLS
        if (m_debugMenu) {
            m_debugMenu->EndSceneReload();
        }
#endif
        if (!ok) {
            gm::core::Logger::Warning("[Game] QuickLoad failed to deserialize scene");
            if (m_toolingFacade) m_toolingFacade->AddNotification("QuickLoad failed");
            gm::core::Event::Trigger(gotmilked::GameEvents::SceneLoadFailed);
            return;
        }

        if (m_gameScene) {
            m_gameScene->BumpReloadVersion();
        }

        auto quickObjects = m_gameScene->GetAllGameObjects();
        gm::core::Logger::Info("[Game] QuickLoad scene object count: {}", quickObjects.size());
        for (const auto& obj : quickObjects) {
            if (!obj) {
                gm::core::Logger::Error("[Game] QuickLoad: encountered null GameObject");
                continue;
            }
            gm::core::Logger::Info("[Game] QuickLoad: raw name '{}'", obj->GetName());
            const std::string& objName = obj->GetName();
            if (objName.empty()) {
                gm::core::Logger::Error("[Game] QuickLoad: GameObject with empty name (ptr {})", static_cast<const void*>(obj.get()));
            } else {
                gm::core::Logger::Info("[Game] QuickLoad: GameObject '{}'", objName);
            }

            auto comps = obj->GetComponents();
            gm::core::Logger::Info("[Game] QuickLoad: '{}' has {} components", objName.c_str(), comps.size());
            for (const auto& comp : comps) {
                if (!comp) {
                    gm::core::Logger::Error("[Game] QuickLoad: '{}' has null component", objName.c_str());
                    continue;
                }
                const std::string& compName = comp->GetName();
                if (compName.empty()) {
                    gm::core::Logger::Error("[Game] QuickLoad: component with empty name on '{}' (typeid {})",
                        objName.c_str(), typeid(*comp).name());
                } else {
                    gm::core::Logger::Info("[Game] QuickLoad:     Component '{}'", compName);
                }
            }
        }
        
        // Apply camera if present
        if (saveJson.contains("camera")) {
            const auto& camera = saveJson["camera"];
            if (camera.contains("position") && camera.contains("forward") && camera.contains("fov")) {
                auto pos = camera["position"];
                auto fwd = camera["forward"];
                if (pos.is_array() && pos.size() == 3 && fwd.is_array() && fwd.size() == 3) {
                    glm::vec3 cameraPos(pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>());
                    glm::vec3 cameraFwd(fwd[0].get<float>(), fwd[1].get<float>(), fwd[2].get<float>());
                    float cameraFov = camera.value("fov", 60.0f);
                    if (m_camera) {
                        m_camera->SetPosition(cameraPos);
                        m_camera->SetForward(cameraFwd);
                    }
            if (m_cameraRigSystem) {
                m_cameraRigSystem->SetFovDegrees(cameraFov);
                    }
                }
            }
        }
        
        // Apply world time if present
        if (saveJson.contains("worldTime") && m_cameraRigSystem) {
            double worldTime = saveJson.value("worldTime", 0.0);
            m_cameraRigSystem->SetWorldTimeSeconds(worldTime);
        }
        
        ApplyResourcesToScene();
        if (m_cameraRigSystem) {
            m_cameraRigSystem->SetSceneContext(m_gameScene);
        }
#if GM_DEBUG_TOOLS
        if (m_terrainEditingSystem) {
            m_terrainEditingSystem->SetSceneContext(m_gameScene);
        }
#endif
        if (m_questSystem) {
            m_questSystem->SetSceneContext(m_gameScene);
        }
        if (m_toolingFacade) {
            m_toolingFacade->AddNotification("QuickLoad applied (with GameObjects)");
        }
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickLoaded);
        return;
    }
    
    // Fall back to old format (no GameObjects)
    m_completedQuests.clear();
    m_completedDialogues.clear();
    if (m_narrativeLog) {
        m_narrativeLog->Clear();
    }
    gm::save::SaveGameData data;
    auto result = m_saveManager->QuickLoad(data);
    if (!result.success) {
        gm::core::Logger::Warning("[Game] QuickLoad failed: {}", result.message);
        if (m_toolingFacade) m_toolingFacade->AddNotification("QuickLoad failed");
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneLoadFailed);
        return;
    }

    if (!data.version.IsCompatibleWith(gm::save::SaveVersion::Current())) {
        gm::core::Logger::Warning(
            "[Game] QuickLoad: legacy save version {} may be incompatible with runtime {}; attempting migration",
            data.version.ToString(), gm::save::SaveVersion::Current().ToString());
    }

#if GM_DEBUG_TOOLS
    if (m_debugMenu) {
        m_debugMenu->BeginSceneReload();
    }
#endif

    bool applied = gm::save::SaveSnapshotHelpers::ApplySnapshot(
        data,
        m_camera.get(),
        m_gameScene,
        [this](double worldTime) {
            if (m_cameraRigSystem) {
                m_cameraRigSystem->SetWorldTimeSeconds(worldTime);
            }
        });

#if GM_DEBUG_TOOLS
    if (m_debugMenu) {
        m_debugMenu->EndSceneReload();
    }
#endif

    if (applied && m_gameScene) {
        if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
            auto terrainObject = m_gameScene->FindGameObjectByName("Terrain");
            if (terrainObject) {
                if (auto terrain = terrainObject->GetComponent<EditableTerrainComponent>()) {
                    bool ok = terrain->SetHeightData(
                        data.terrainResolution,
                        data.terrainSize,
                        data.terrainMinHeight,
                        data.terrainMaxHeight,
                        data.terrainHeights);
                    if (!ok) {
                        gm::core::Logger::Warning("[Game] Failed to apply terrain data from save");
                    } else {
                        terrain->SetTextureTiling(data.terrainTextureTiling);
                        terrain->SetBaseTextureGuidFromSave(data.terrainBaseTextureGuid);

                        const auto& layers = data.terrainPaintLayers;
                        terrain->SetPaintLayerCount(std::max(1, static_cast<int>(layers.size())));
                        for (std::size_t i = 0; i < layers.size() && i < static_cast<std::size_t>(gm::debug::EditableTerrainComponent::kMaxPaintLayers); ++i) {
                            const auto& layer = layers[i];
                            terrain->SetPaintLayerData(static_cast<int>(i), layer.guid, layer.enabled, layer.weights);
                        }
                        terrain->SetActivePaintLayerIndex(data.terrainActivePaintLayer);
                    }
                }
            }
        }
    }
    
    // Apply FOV if present
    if (data.cameraFov > 0.0f && m_cameraRigSystem) {
        m_cameraRigSystem->SetFovDegrees(data.cameraFov);
    }
    
    ApplyResourcesToScene();
    if (m_cameraRigSystem) {
        m_cameraRigSystem->SetSceneContext(m_gameScene);
    }
#if GM_DEBUG_TOOLS
    if (m_terrainEditingSystem) {
        m_terrainEditingSystem->SetSceneContext(m_gameScene);
    }
#endif
    if (m_questSystem) {
        m_questSystem->SetSceneContext(m_gameScene);
    }
    if (m_toolingFacade) {
        m_toolingFacade->AddNotification(applied ? "QuickLoad applied" : "QuickLoad partially applied");
    }

    if (m_gameScene) {
        m_gameScene->BumpReloadVersion();
    }
    
    // Trigger event for successful load
    if (applied) {
        gm::core::Event::Trigger(gotmilked::GameEvents::SceneQuickLoaded);
    }
}

void Game::ForceResourceReload() {
    bool ok = m_resources.ReloadAll();
    ApplyResourcesToScene();
    m_hotReloader.ForcePoll();
    if (ok) {
        gm::core::Logger::Info("[Game] Resources reloaded");
        if (m_toolingFacade) m_toolingFacade->AddNotification("Resources reloaded");
    } else {
        gm::core::Logger::Warning("[Game] Resource reload encountered errors");
        if (m_toolingFacade) m_toolingFacade->AddNotification("Resource reload failed");
    }
}

bool Game::SetupPrefabs() {
    m_prefabLibrary = std::make_shared<gm::scene::PrefabLibrary>();
    if (m_toolingFacade) {
        m_prefabLibrary->SetMessageCallback([this](const std::string& message, bool isError) {
            if (!m_toolingFacade) {
                return;
            }
            const std::string formatted = isError
                ? fmt::format("Prefab error: {}", message)
                : fmt::format("Prefab warning: {}", message);
            m_toolingFacade->AddNotification(formatted);
        });
    }
    std::filesystem::path prefabRoot = m_assetsDir / "prefabs";
    if (!m_prefabLibrary->LoadDirectory(prefabRoot)) {
        gm::core::Logger::Info("[Game] No prefabs loaded from {}", prefabRoot.string());
    }
    return true;
}

