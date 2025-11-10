#include "gm/utils/ImGuiManager.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <cstdio>

namespace gm {
namespace utils {

ImGuiManager::ImGuiManager()
    : m_window(nullptr)
    , m_initialized(false) {
}

ImGuiManager::~ImGuiManager() {
    if (m_initialized) {
        Shutdown();
    }
}

bool ImGuiManager::Init(GLFWwindow* window) {
    if (m_initialized) return true;
    
    if (!window) {
        printf("[ImGuiManager] Error: Window is null\n");
        return false;
    }
    
    m_window = window;
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");
    
    m_initialized = true;
    printf("[ImGuiManager] Initialized successfully\n");
    return true;
}

void ImGuiManager::NewFrame() {
    if (!m_initialized) return;
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Render() {
    if (!m_initialized) return;
    
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiManager::Shutdown() {
    if (!m_initialized) return;
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    m_initialized = false;
    m_window = nullptr;
}

} // namespace utils
} // namespace gm

