#include "gm/utils/CoordinateDisplay.hpp"
#include "gm/rendering/Camera.hpp"
#include "imgui.h"
#include <iomanip>
#include <sstream>

namespace gm {
namespace utils {

CoordinateDisplay::CoordinateDisplay()
    : m_visible(true) {
}

void CoordinateDisplay::Render(const Camera& camera, float fov) {
    if (!m_visible) return;
    
    // Get camera position
    const glm::vec3& pos = camera.Position();
    
    // Set window position and size
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_FirstUseEver);
    
    // Create window
    ImGui::Begin("Camera Info", nullptr, 
                 ImGuiWindowFlags_NoMove | 
                 ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_AlwaysAutoResize);
    
    // Display camera position
    ImGui::Text("Camera Position:");
    ImGui::Text("X: %.2f", pos.x);
    ImGui::Text("Y: %.2f", pos.y);
    ImGui::Text("Z: %.2f", pos.z);
    
    ImGui::Separator();
    
    // Display FOV
    ImGui::Text("FOV: %.2f deg", fov);
    
    ImGui::End();
}

} // namespace utils
} // namespace gm
