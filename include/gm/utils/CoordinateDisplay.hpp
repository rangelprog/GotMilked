#pragma once
#include <glm/glm.hpp>

namespace gm {
class Camera;
}

namespace gm {
namespace utils {

/**
 * @brief Utility for displaying camera coordinates using ImGui
 * 
 * Renders camera position and FOV in the top-left corner using ImGui.
 */
class CoordinateDisplay {
public:
    CoordinateDisplay();
    ~CoordinateDisplay() = default;

    // Render coordinates (call each frame after ImGui::NewFrame())
    void Render(const Camera& camera, float fov);

    // Set visibility
    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisible() const { return m_visible; }

private:
    bool m_visible;
};

} // namespace utils
} // namespace gm
