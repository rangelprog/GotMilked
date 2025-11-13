#pragma once

#include <string>
#include <glm/mat4x4.hpp>

namespace gm {
namespace core {
class Input;
}
namespace utils {
class ImGuiManager;
}
namespace tooling {
class Overlay;
}
#if GM_DEBUG_TOOLS
namespace debug {
class EditableTerrainComponent;
}
#endif
} // namespace gm

class Game;

class ToolingFacade {
public:
    explicit ToolingFacade(Game& game);
    ~ToolingFacade() = default;

    bool IsImGuiReady() const;
    bool WantsKeyboardInput() const;
    bool WantsAnyInput() const;

    bool HandleOverlayToggle();
    bool IsOverlayActive() const;
    bool DebugMenuHasSelection() const;
    bool ShouldBlockCameraInput() const;

    void AddNotification(const std::string& message);
    void RefreshHud();
    void UpdateSceneReference();
#if GM_DEBUG_TOOLS
    void RegisterTerrain(gm::debug::EditableTerrainComponent* terrain);
#endif

    void HandleDebugShortcuts(gm::core::Input& input);

    void BeginFrame();
    void RenderGrid(const glm::mat4& view, const glm::mat4& proj);
    void RenderUI();

    void Shutdown();

    gm::utils::ImGuiManager* ImGui();
    gm::tooling::Overlay* Overlay();

private:
    Game& m_game;
};

