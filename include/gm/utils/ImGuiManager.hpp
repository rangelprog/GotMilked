#pragma once

struct GLFWwindow;

namespace gm {
namespace utils {

/**
 * @brief Manager for Dear ImGui integration
 * 
 * Handles initialization, rendering, and cleanup of ImGui.
 */
class ImGuiManager {
public:
    ImGuiManager();
    ~ImGuiManager();

    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

    // Initialize ImGui with GLFW and OpenGL
    bool Init(GLFWwindow* window);

    // Start a new frame (call at beginning of render loop)
    void NewFrame();

    // Render ImGui (call at end of render loop, after all ImGui calls)
    void Render();

    // Shutdown ImGui
    void Shutdown();

    // Check if initialized
    bool IsInitialized() const { return m_initialized; }

private:
    GLFWwindow* m_window;
    bool m_initialized;
};

} // namespace utils
} // namespace gm

