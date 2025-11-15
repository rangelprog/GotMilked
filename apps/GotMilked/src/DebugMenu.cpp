#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "gm/scene/Scene.hpp"
#include "GameResources.hpp"
#include "gm/core/Logger.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <cstring>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
inline bool IsImGuiKeyPressed(ImGuiKey key) {
    return ImGui::IsKeyPressed(key);
}

inline bool IsImGuiKeyDown(ImGuiKey key) {
    return ImGui::IsKeyDown(key);
}
#else
inline bool IsImGuiKeyPressed(ImGuiKey key) {
    return ImGui::IsKeyPressed(ImGui::GetKeyIndex(key));
}

inline bool IsImGuiKeyDown(ImGuiKey key) {
    return ImGui::IsKeyDown(ImGui::GetKeyIndex(key));
}
#endif

std::string ToUpperCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

ImGuiKey KeyFromString(const std::string& name) {
    if (name.empty()) {
        return ImGuiKey_None;
    }

    const std::string upper = ToUpperCopy(name);

#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
    for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
        if (const char* label = ImGui::GetKeyName(static_cast<ImGuiKey>(key))) {
            if (label[0] == '\0') {
                continue;
            }
            if (upper == ToUpperCopy(label)) {
                return static_cast<ImGuiKey>(key);
            }
        }
    }
#endif

    if (upper.size() == 1) {
        char c = upper[0];
        if (c >= 'A' && c <= 'Z') {
            return static_cast<ImGuiKey>(ImGuiKey_A + (c - 'A'));
        }
        if (c >= '0' && c <= '9') {
            return static_cast<ImGuiKey>(ImGuiKey_0 + (c - '0'));
        }
    }

    return ImGuiKey_None;
}

std::string KeyToString(ImGuiKey key) {
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
    if (const char* label = ImGui::GetKeyName(key)) {
        if (label[0] != '\0') {
            return label;
        }
    }
#endif
    return std::string();
}

} // namespace

namespace gm::debug {

using json = nlohmann::json;

DebugMenu::DebugMenu() {
    InitializeShortcutDefaults();
    m_timeOfDayTimeline.durationSeconds = 120.0f;
    m_timeOfDayTimeline.keyframes.push_back({0.0f, 0.0f});
    m_timeOfDayTimeline.keyframes.push_back({m_timeOfDayTimeline.durationSeconds, 1.0f});
    EnsureWeatherScenarioDefaults();
}

DebugMenu::~DebugMenu() {
    if (!m_layoutProfilePath.empty()) {
        SaveLayoutProfileInternal(m_layoutProfilePath);
    }
    UnloadPlugins();
}

void DebugMenu::Render(bool& menuVisible) {
    if (!menuVisible) {
        return;
    }

    ProcessGlobalShortcuts();

    if (m_sceneReloadInProgress) {
        if (m_sceneReloadPendingResume) {
            m_sceneReloadInProgress = false;
            m_sceneReloadPendingResume = false;
        }
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        RenderMenuBar();
        ImGui::EndMainMenuBar();
    }

    RenderDockspace();

    if (m_pendingSaveAs) {
        m_pendingSaveAs = false;
        HandleSaveAs();
    }

    if (m_pendingLoad) {
        m_pendingLoad = false;
        HandleLoad();
    }

    if (m_pendingImport) {
        m_pendingImport = false;
        m_showImportDialog = true;
        // Initialize import settings with defaults
        if (m_importSettings.inputPath.empty() && m_gameResources) {
            std::filesystem::path assetsDir = m_gameResources->GetAssetsDirectory();
            m_importSettings.outputDir = assetsDir / "models";
        }
    }

    if (m_showImportDialog) {
        RenderImportModelDialog();
    }

    if (m_showSceneExplorer) {
        RenderSceneExplorerWindow();
        RenderTransformGizmo();
    }

    if (m_showSaveAsDialog) {
        RenderSaveAsDialog();
    }

    if (m_showLoadDialog) {
        RenderLoadDialog();
    }

    if (m_showSceneInfo) {
        RenderSceneInfo();
    }

    if (m_showPrefabBrowser) {
        RenderPrefabBrowser();
    }

    if (m_showContentBrowser) {
        RenderContentBrowser();
    }

    if (m_showAnimationDebugger) {
        RenderAnimationDebugger();
    }

    if (m_showContentValidation) {
        RenderContentValidationWindow();
    }

    if (m_showCelestialDebugger) {
        RenderCelestialDebugger();
    }
    if (m_showFogDebugger) {
        RenderFogDebugger();
    }
    if (m_showWeatherPanel && m_weatherDiagnosticsSystem) {
        RenderWeatherPanel(*m_weatherDiagnosticsSystem);
    }
    if (m_showWeatherScenarioEditor) {
        RenderWeatherScenarioEditor();
    }

    RenderPluginWindows();

    RenderGameObjectOverlay();

    if (m_showDebugConsole && m_debugConsole) {
        bool open = m_showDebugConsole;
        m_debugConsole->Render(&open);
        m_showDebugConsole = open;
    }

    AutosaveLayout(ImGui::GetIO().DeltaTime);
}

void DebugMenu::SetConsoleVisible(bool visible) {
    m_showDebugConsole = visible;
}

bool DebugMenu::IsConsoleVisible() const {
    return m_showDebugConsole;
}

void DebugMenu::SetOverlayToggleCallbacks(std::function<bool()> getter, std::function<void(bool)> setter) {
    m_overlayGetter = std::move(getter);
    m_overlaySetter = std::move(setter);
}

void DebugMenu::ProcessGlobalShortcuts() {
    ImGuiIO& io = ImGui::GetIO();

    if (m_sceneReloadInProgress) {
        m_suppressCameraInput = false;
        return;
    }

    auto selected = m_selectedGameObject.lock();
    if (selected && IsImGuiKeyPressed(ImGuiKey_Escape)) {
        ClearSelection();
        selected.reset();
    }

    bool hasSelection = static_cast<bool>(selected);
    bool allowHotkeys = !io.WantCaptureKeyboard && !m_sceneReloadInProgress && m_sceneReloadFramesToSkip == 0;

    auto triggerShortcut = [&](const char* id) {
        auto it = m_shortcutHandlers.find(id);
        if (it != m_shortcutHandlers.end() && IsShortcutPressed(it->second.binding)) {
            if (it->second.callback) {
                it->second.callback();
            }
            return true;
        }
        return false;
    };

    if (allowHotkeys) {
        triggerShortcut("gizmo_translate");
        triggerShortcut("gizmo_rotate");
        triggerShortcut("gizmo_scale");
        triggerShortcut("undo");
        triggerShortcut("redo");

        for (auto& [id, handler] : m_shortcutHandlers) {
            if (!handler.fromPlugin) {
                continue;
            }
            if (IsShortcutPressed(handler.binding) && handler.callback) {
                handler.callback();
            }
        }
    }

    m_suppressCameraInput = hasSelection && !io.WantCaptureKeyboard && !io.WantCaptureMouse && !m_sceneReloadInProgress && m_sceneReloadFramesToSkip == 0;
}

bool DebugMenu::ShouldBlockCameraInput() const {
    return m_suppressCameraInput;
}

bool DebugMenu::HasSelection() const {
    return !m_selectedGameObject.expired();
}

void DebugMenu::ClearSelection() {
    m_selectedGameObject.reset();
    m_suppressCameraInput = false;
}

void DebugMenu::BeginSceneReload() {
    m_sceneReloadInProgress = true;
    m_sceneReloadPendingResume = false;
    ClearSelection();
}

void DebugMenu::EndSceneReload() {
    m_sceneReloadPendingResume = true;
    m_sceneReloadFramesToSkip = 1;
    if (auto scene = m_scene.lock()) {
        scene->BumpReloadVersion();
    }
}

bool DebugMenu::ShouldDelaySceneUI() {
    if (!m_sceneReloadInProgress && !m_sceneReloadPendingResume && m_sceneReloadFramesToSkip == 0) {
        auto scene = m_scene.lock();
        if (scene && scene->GetAllGameObjects().empty()) {
            m_sceneReloadPendingResume = true;
            m_sceneReloadFramesToSkip = 1;
            return true;
        }
        return false;
    }

    if (m_sceneReloadFramesToSkip > 0) {
        --m_sceneReloadFramesToSkip;
        if (m_sceneReloadFramesToSkip == 0) {
            m_sceneReloadPendingResume = false;
            m_sceneReloadInProgress = false;
        }
        return true;
    }

    auto scene = m_scene.lock();
    if (scene && scene->GetAllGameObjects().empty()) {
        m_sceneReloadFramesToSkip = 1;
        m_sceneReloadPendingResume = true;
        return true;
    }

    return false;
}

void DebugMenu::SetLayoutProfilePath(const std::filesystem::path& path) {
    m_layoutProfilePath = path;
    if (m_layoutProfilePath.empty()) {
        return;
    }

    std::error_code ec;
    if (!m_layoutProfilePath.parent_path().empty()) {
        std::filesystem::create_directories(m_layoutProfilePath.parent_path(), ec);
    }
    LoadLayoutProfileInternal(m_layoutProfilePath);
}

void DebugMenu::SetPluginManifestPath(const std::filesystem::path& path) {
    m_pluginManifestPath = path;
    ReloadPlugins();
}

void DebugMenu::ReloadPlugins() {
    LoadPluginsFromManifest();
}

void DebugMenu::RegisterDockWindow(const std::string& id,
                                   const std::string& title,
                                   const std::function<void()>& renderFn,
                                   bool* visibilityFlag) {
    if (id.empty() || !renderFn) {
        return;
    }

    PluginWindow window;
    window.id = id;
    window.title = title.empty() ? id : title;
    window.renderFn = renderFn;
    window.externalVisibility = visibilityFlag;
    window.owner = m_activePlugin;

    const auto stateKey = std::string("plugin:") + id;
    if (auto it = m_windowStateOverrides.find(stateKey); it != m_windowStateOverrides.end()) {
        if (window.externalVisibility) {
            *window.externalVisibility = it->second;
        } else {
            window.visible = it->second;
        }
    }

    m_pluginWindows.push_back(window);
    MarkLayoutDirty();
}

void DebugMenu::RegisterShortcut(const ShortcutDesc& desc, const std::function<void()>& handler) {
    if (desc.id.empty() || !handler) {
        return;
    }

    ShortcutBinding binding = ShortcutFromDesc(desc);
    if (auto it = m_shortcutOverrides.find(desc.id); it != m_shortcutOverrides.end()) {
        binding = it->second;
    }
    RegisterShortcutHandler(desc.id, binding, handler, true, m_activePlugin);
}

void DebugMenu::PushUndoableAction(const std::string& description,
                                   const std::function<void()>& redo,
                                   const std::function<void()>& undo) {
    if (!redo || !undo) {
        return;
    }
    if (m_undoStack.size() >= m_maxUndoDepth) {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_undoStack.push_back(EditorAction{redo, undo, description});
    m_redoStack.clear();
}

void DebugMenu::InitializeShortcutDefaults() {
    m_shortcutHandlers.clear();
    m_shortcutOverrides.clear();

    RegisterShortcutHandler("gizmo_translate", ShortcutBinding{ImGuiKey_W, false, false, false}, [this]() {
        m_gizmoOperation = 0;
    });
    RegisterShortcutHandler("gizmo_rotate", ShortcutBinding{ImGuiKey_E, false, false, false}, [this]() {
        m_gizmoOperation = 1;
    });
    RegisterShortcutHandler("gizmo_scale", ShortcutBinding{ImGuiKey_R, false, false, false}, [this]() {
        m_gizmoOperation = 2;
    });
    RegisterShortcutHandler("undo", ShortcutBinding{ImGuiKey_Z, true, false, false}, [this]() {
        UndoLastAction();
    });
    RegisterShortcutHandler("redo", ShortcutBinding{ImGuiKey_Y, true, false, false}, [this]() {
        RedoLastAction();
    });
}

void DebugMenu::RegisterShortcutHandler(const std::string& id,
                                        ShortcutBinding binding,
                                        const std::function<void()>& handler,
                                        bool fromPlugin,
                                        EditorPlugin* owner) {
    ShortcutHandler& entry = m_shortcutHandlers[id];
    entry.binding = binding;
    entry.callback = handler;
    entry.fromPlugin = fromPlugin;
    entry.owner = owner;
}

bool DebugMenu::IsShortcutPressed(const ShortcutBinding& binding) const {
    if (binding.key == ImGuiKey_None) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (binding.ctrl && !io.KeyCtrl) {
        return false;
    }
    if (binding.shift && !io.KeyShift) {
        return false;
    }
    if (binding.alt && !io.KeyAlt) {
        return false;
    }

#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
    return ImGui::IsKeyPressed(binding.key);
#else
    return ImGui::IsKeyPressed(ImGui::GetKeyIndex(binding.key));
#endif
}

bool DebugMenu::IsShortcutPressed(const std::string& id) const {
    auto it = m_shortcutHandlers.find(id);
    if (it == m_shortcutHandlers.end()) {
        return false;
    }
    return IsShortcutPressed(it->second.binding);
}

std::string DebugMenu::FormatShortcutLabel(const ShortcutBinding& binding) const {
    std::string label;
    if (binding.ctrl) {
        label += "Ctrl+";
    }
    if (binding.shift) {
        label += "Shift+";
    }
    if (binding.alt) {
        label += "Alt+";
    }
    label += KeyToString(binding.key);
    return label;
}

std::vector<std::pair<std::string, bool*>> DebugMenu::GetWindowBindings() {
    return {
        {"sceneExplorer", &m_showSceneExplorer},
        {"sceneInfo", &m_showSceneInfo},
        {"prefabBrowser", &m_showPrefabBrowser},
        {"contentBrowser", &m_showContentBrowser},
        {"animationDebugger", &m_showAnimationDebugger},
        {"debugConsole", &m_showDebugConsole},
        {"weatherScenarioEditor", &m_showWeatherScenarioEditor},
        {"weatherDiagnostics", &m_showWeatherPanel},
        {"celestialDebugger", &m_showCelestialDebugger}
    };
}

void DebugMenu::MarkLayoutDirty() {
    m_layoutDirty = true;
    m_layoutAutosaveTimer = 0.0f;
}

void DebugMenu::AutosaveLayout(float deltaTime) {
    if (!m_layoutDirty || m_layoutProfilePath.empty()) {
        return;
    }
    m_layoutAutosaveTimer += deltaTime;
    if (m_layoutAutosaveTimer >= m_layoutAutosaveInterval) {
        SaveLayoutProfileInternal(m_layoutProfilePath);
        m_layoutAutosaveTimer = 0.0f;
        m_layoutDirty = false;
    }
}

bool DebugMenu::SaveLayoutProfileInternal(const std::filesystem::path& path) const {
    if (path.empty()) {
        return false;
    }

    json data;
    data["version"] = 1;

    if (ImGui::GetCurrentContext()) {
        const char* iniData = ImGui::SaveIniSettingsToMemory(nullptr);
        data["dockspace"] = iniData ? std::string(iniData) : std::string();
    }

    auto& windows = data["windows"];
    auto bindings = const_cast<DebugMenu*>(this)->GetWindowBindings();
    for (const auto& binding : bindings) {
        if (binding.second) {
            windows[binding.first] = *binding.second;
        }
    }
    for (const auto& pluginWindow : m_pluginWindows) {
        bool visible = pluginWindow.externalVisibility ? *pluginWindow.externalVisibility : pluginWindow.visible;
        windows["plugin:" + pluginWindow.id] = visible;
    }

    auto& shortcuts = data["shortcuts"];
    for (const auto& [id, handler] : m_shortcutHandlers) {
        shortcuts[id] = ShortcutToJson(handler.binding);
    }

    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream file(path);
    if (!file) {
        gm::core::Logger::Error("[DebugMenu] Failed to save layout profile {}", path.string());
        return false;
    }

    file << data.dump(2);
    return true;
}

bool DebugMenu::LoadLayoutProfileInternal(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        gm::core::Logger::Warning("[DebugMenu] Layout profile {} not found", path.string());
        return false;
    }

    json data = json::parse(file, nullptr, false);
    if (data.is_discarded()) {
        gm::core::Logger::Error("[DebugMenu] Failed to parse layout profile {}", path.string());
        return false;
    }

    if (data.contains("dockspace") && data["dockspace"].is_string()) {
        m_cachedDockspaceLayout = data["dockspace"].get<std::string>();
        m_pendingDockRestore = true;
    }

    m_windowStateOverrides.clear();
    if (data.contains("windows")) {
        for (const auto& [key, value] : data["windows"].items()) {
            if (value.is_boolean()) {
                m_windowStateOverrides[key] = value.get<bool>();
            }
        }
    }
    ApplyWindowStateOverrides();

    m_shortcutOverrides.clear();
    if (data.contains("shortcuts")) {
        for (const auto& [key, value] : data["shortcuts"].items()) {
            m_shortcutOverrides[key] = ShortcutFromJson(value);
        }
    }
    ApplyShortcutOverrides();

    m_layoutDirty = false;
    return true;
}

void DebugMenu::ApplyWindowStateOverrides() {
    for (auto& binding : GetWindowBindings()) {
        if (auto it = m_windowStateOverrides.find(binding.first); it != m_windowStateOverrides.end()) {
            if (binding.second) {
                *binding.second = it->second;
            }
        }
    }
}

void DebugMenu::ApplyShortcutOverrides() {
    for (const auto& [id, binding] : m_shortcutOverrides) {
        if (auto it = m_shortcutHandlers.find(id); it != m_shortcutHandlers.end()) {
            it->second.binding = binding;
        }
    }
}

DebugMenu::ShortcutBinding DebugMenu::ShortcutFromDesc(const ShortcutDesc& desc) const {
    ShortcutBinding binding;
    binding.key = KeyFromString(desc.key);
    binding.ctrl = desc.ctrl;
    binding.shift = desc.shift;
    binding.alt = desc.alt;
    return binding;
}

DebugMenu::ShortcutBinding DebugMenu::ShortcutFromJson(const json& data) const {
    ShortcutBinding binding;
    if (!data.is_object()) {
        return binding;
    }
    binding.key = KeyFromString(data.value("key", ""));
    binding.ctrl = data.value("ctrl", false);
    binding.shift = data.value("shift", false);
    binding.alt = data.value("alt", false);
    return binding;
}

json DebugMenu::ShortcutToJson(const ShortcutBinding& binding) const {
    json data;
    data["key"] = KeyToString(binding.key);
    data["ctrl"] = binding.ctrl;
    data["shift"] = binding.shift;
    data["alt"] = binding.alt;
    return data;
}

bool DebugMenu::UndoLastAction() {
    if (m_undoStack.empty()) {
        return false;
    }

    auto action = m_undoStack.back();
    m_undoStack.pop_back();
    if (action.undo) {
        action.undo();
    }
    m_redoStack.push_back(action);
    return true;
}

bool DebugMenu::RedoLastAction() {
    if (m_redoStack.empty()) {
        return false;
    }

    auto action = m_redoStack.back();
    m_redoStack.pop_back();
    if (action.redo) {
        action.redo();
    }
    m_undoStack.push_back(action);
    return true;
}

void DebugMenu::HandlePluginMenu() {
    if (!ImGui::BeginMenu("Plugins")) {
        return;
    }

    if (m_pluginWindows.empty()) {
        ImGui::MenuItem("No plugin windows available", nullptr, false, false);
    } else {
        for (auto& window : m_pluginWindows) {
            bool open = window.externalVisibility ? *window.externalVisibility : window.visible;
            if (ImGui::MenuItem(window.title.c_str(), nullptr, open)) {
                if (window.externalVisibility) {
                    *window.externalVisibility = !open;
                } else {
                    window.visible = !open;
                }
                MarkLayoutDirty();
            }
        }
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Reload Plugins")) {
        ReloadPlugins();
    }

    ImGui::EndMenu();
}

void DebugMenu::RenderPluginWindows() {
    for (auto& plugin : m_plugins) {
        if (!plugin.instance) {
            continue;
        }
        m_activePlugin = plugin.instance;
        plugin.instance->Render(*this);
        m_activePlugin = nullptr;
    }

    for (auto& window : m_pluginWindows) {
        bool* statePtr = window.externalVisibility ? window.externalVisibility : &window.visible;
        bool open = statePtr ? *statePtr : window.visible;
        if (!open) {
            continue;
        }

        bool initialState = open;
        if (ImGui::Begin(window.title.c_str(), statePtr)) {
            if (window.renderFn) {
                window.renderFn();
            }
        }
        ImGui::End();

        bool updatedState = statePtr ? *statePtr : window.visible;
        if (!window.externalVisibility) {
            window.visible = updatedState;
        }
        if (updatedState != initialState) {
            MarkLayoutDirty();
        }
    }
}

void DebugMenu::LoadPluginsFromManifest() {
    UnloadPlugins();

    if (m_pluginManifestPath.empty()) {
        return;
    }

    std::ifstream file(m_pluginManifestPath);
    if (!file) {
        gm::core::Logger::Warning("[DebugMenu] Plugin manifest {} not found", m_pluginManifestPath.string());
        return;
    }

    json manifest = json::parse(file, nullptr, false);
    if (manifest.is_discarded()) {
        gm::core::Logger::Error("[DebugMenu] Failed to parse plugin manifest {}", m_pluginManifestPath.string());
        return;
    }

    if (!manifest.contains("plugins") || !manifest["plugins"].is_array()) {
        gm::core::Logger::Warning("[DebugMenu] Plugin manifest missing 'plugins' array");
        return;
    }

    auto baseDir = m_pluginManifestPath.parent_path();
    for (const auto& entry : manifest["plugins"]) {
        if (!entry.is_object()) {
            continue;
        }

        std::filesystem::path library = entry.value("library", "");
        if (library.empty()) {
            continue;
        }
        if (library.is_relative()) {
            library = baseDir / library;
        }

        LoadedPlugin plugin;
        plugin.name = entry.value("name", library.stem().string());
        plugin.path = library;
        plugin.handle = LoadLibraryHandle(library);
        if (!plugin.handle) {
            gm::core::Logger::Error("[DebugMenu] Failed to load plugin {} ({})", plugin.name, library.string());
            continue;
        }

        auto create = reinterpret_cast<CreateEditorPluginFn>(ResolveSymbol(plugin.handle, "GM_CreateEditorPlugin"));
        auto destroy = reinterpret_cast<DestroyEditorPluginFn>(ResolveSymbol(plugin.handle, "GM_DestroyEditorPlugin"));
        if (!create || !destroy) {
            gm::core::Logger::Error("[DebugMenu] Plugin {} missing factory exports", plugin.name);
            UnloadLibraryHandle(plugin.handle);
            continue;
        }

        plugin.destroy = destroy;
        plugin.instance = create();
        if (!plugin.instance) {
            gm::core::Logger::Error("[DebugMenu] Plugin {} failed to create instance", plugin.name);
            UnloadLibraryHandle(plugin.handle);
            continue;
        }

        m_activePlugin = plugin.instance;
        plugin.instance->Initialize(*this);
        m_activePlugin = nullptr;

        m_plugins.push_back(std::move(plugin));
    }
}

void DebugMenu::RemovePluginArtifacts(EditorPlugin* plugin) {
    if (!plugin) {
        return;
    }

    for (auto it = m_shortcutHandlers.begin(); it != m_shortcutHandlers.end();) {
        if (it->second.owner == plugin) {
            it = m_shortcutHandlers.erase(it);
        } else {
            ++it;
        }
    }

    m_pluginWindows.erase(
        std::remove_if(m_pluginWindows.begin(), m_pluginWindows.end(),
                       [plugin](const PluginWindow& window) {
                           return window.owner == plugin;
                       }),
        m_pluginWindows.end());
}

void DebugMenu::UnloadPlugins() {
    for (auto& plugin : m_plugins) {
        if (plugin.instance) {
            RemovePluginArtifacts(plugin.instance);
            m_activePlugin = plugin.instance;
            plugin.instance->Shutdown(*this);
            m_activePlugin = nullptr;
            if (plugin.destroy) {
                plugin.destroy(plugin.instance);
            }
        }
        if (plugin.handle) {
            UnloadLibraryHandle(plugin.handle);
        }
    }
    m_plugins.clear();
    m_pluginWindows.clear();

    for (auto it = m_shortcutHandlers.begin(); it != m_shortcutHandlers.end();) {
        if (it->second.fromPlugin) {
            it = m_shortcutHandlers.erase(it);
        } else {
            ++it;
        }
    }
}

void* DebugMenu::LoadLibraryHandle(const std::filesystem::path& path) {
#ifdef _WIN32
    return static_cast<void*>(::LoadLibraryW(path.wstring().c_str()));
#else
    return dlopen(path.string().c_str(), RTLD_NOW);
#endif
}

void DebugMenu::UnloadLibraryHandle(void* handle) {
    if (!handle) {
        return;
    }
#ifdef _WIN32
    ::FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

void* DebugMenu::ResolveSymbol(void* handle, const char* name) {
    if (!handle || !name) {
        return nullptr;
    }
#ifdef _WIN32
    return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    return dlsym(handle, name);
#endif
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS

