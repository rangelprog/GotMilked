#if defined(GM_DEBUG) || defined(_DEBUG)

#include "gm/tooling/DebugConsole.hpp"

#include <imgui.h>
#include <fmt/format.h>

namespace gm::tooling {

namespace {
ImVec4 LevelColor(core::LogLevel level) {
    switch (level) {
        case core::LogLevel::Error:   return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        case core::LogLevel::Warning: return ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
        case core::LogLevel::Info:    return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
        case core::LogLevel::Debug:
        default:                      return ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
    }
}
} // namespace

DebugConsole::DebugConsole() {
    m_entries.reserve(256);
    m_listenerToken = core::Logger::RegisterListener(
        [this](core::LogLevel level, const std::string& line) {
            m_entries.push_back({level, line, std::chrono::system_clock::now()});
            m_scrollToBottom = true;
        });
}

DebugConsole::~DebugConsole() {
    core::Logger::UnregisterListener(m_listenerToken);
}

void DebugConsole::Clear() {
    m_entries.clear();
}

void DebugConsole::Render(bool* open) {
    if (!open) {
        return;
    }

    if (!ImGui::Begin("Debug Console", open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::MenuItem("Clear")) {
            Clear();
        }
        ImGui::Separator();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::EndMenuBar();
    }

    ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& entry : m_entries) {
        ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.level));
        ImGui::TextUnformatted(entry.message.c_str());
        ImGui::PopStyleColor();
    }
    if (m_scrollToBottom || (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
        ImGui::SetScrollHereY(1.0f);
        m_scrollToBottom = false;
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace gm::tooling

#endif // GM_DEBUG || _DEBUG

