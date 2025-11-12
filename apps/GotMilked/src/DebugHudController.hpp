#pragma once

#if GM_DEBUG_TOOLS

#include <vector>
#include <algorithm>

namespace gm {
namespace tooling {
class Overlay;
}
namespace debug {
class DebugMenu;
class DebugConsole;
class EditableTerrainComponent;
}
}

namespace gm::debug {

class DebugHudController {
public:
    DebugHudController() = default;

    void SetDebugMenu(DebugMenu* menu);
    void SetDebugConsole(DebugConsole* console);
    void SetOverlay(gm::tooling::Overlay* overlay);

    void RegisterTerrain(EditableTerrainComponent* terrain);

    void ToggleHud();
    void SetHudVisible(bool visible);
    bool IsHudVisible() const { return m_hudVisible; }

    void RenderHud();
    void RenderTerrainEditors();

    bool GetMenuVisible() const { return m_menuVisible; }
    bool GetConsoleVisible() const { return m_consoleVisible; }
    void SetConsoleVisible(bool visible);

    bool GetOverlayVisible() const { return m_overlayVisible; }
    void SetOverlayVisible(bool visible);

    bool GetTerrainEditingEnabled() const { return m_terrainEditingEnabled; }
    void SetTerrainEditingEnabled(bool enabled);
    void Refresh();

private:
    void ApplyVisibility();

    DebugMenu* m_menu = nullptr;
    DebugConsole* m_console = nullptr;
    gm::tooling::Overlay* m_overlay = nullptr;
    std::vector<EditableTerrainComponent*> m_terrains;

    bool m_hudVisible = false;
    bool m_menuVisible = false;
    bool m_consoleVisible = false;
    bool m_overlayVisible = false;
    bool m_terrainEditingEnabled = false;
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


