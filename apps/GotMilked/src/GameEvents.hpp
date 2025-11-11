#pragma once

#include <string>

namespace gotmilked {

/**
 * @brief Event name constants for the game event system
 * 
 * These constants define all events that can be triggered in the game.
 * Using constants prevents typos and makes event names discoverable.
 */
namespace GameEvents {

// Resource Events
constexpr const char* ResourceShaderLoaded = "resource.shader.loaded";
constexpr const char* ResourceShaderReloaded = "resource.shader.reloaded";
constexpr const char* ResourceTextureLoaded = "resource.texture.loaded";
constexpr const char* ResourceTextureReloaded = "resource.texture.reloaded";
constexpr const char* ResourceMeshLoaded = "resource.mesh.loaded";
constexpr const char* ResourceMeshReloaded = "resource.mesh.reloaded";
constexpr const char* ResourceAllReloaded = "resource.all.reloaded";
constexpr const char* ResourceLoadFailed = "resource.load.failed";

// Scene Events
constexpr const char* SceneSaved = "scene.saved";
constexpr const char* SceneLoaded = "scene.loaded";
constexpr const char* SceneSaveFailed = "scene.save.failed";
constexpr const char* SceneLoadFailed = "scene.load.failed";
constexpr const char* SceneQuickSaved = "scene.quick_saved";
constexpr const char* SceneQuickLoaded = "scene.quick_loaded";

// Hot Reload Events
constexpr const char* HotReloadShaderDetected = "hot_reload.shader.detected";
constexpr const char* HotReloadTextureDetected = "hot_reload.texture.detected";
constexpr const char* HotReloadMeshDetected = "hot_reload.mesh.detected";
constexpr const char* HotReloadShaderReloaded = "hot_reload.shader.reloaded";
constexpr const char* HotReloadTextureReloaded = "hot_reload.texture.reloaded";
constexpr const char* HotReloadMeshReloaded = "hot_reload.mesh.reloaded";

// Gameplay Events
constexpr const char* GameInitialized = "game.initialized";
constexpr const char* GameShutdown = "game.shutdown";
constexpr const char* GamePaused = "game.paused";
constexpr const char* GameResumed = "game.resumed";

} // namespace GameEvents

} // namespace gotmilked

