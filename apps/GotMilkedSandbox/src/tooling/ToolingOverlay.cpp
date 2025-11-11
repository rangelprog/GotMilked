#include "ToolingOverlay.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "imgui.h"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"
#include "../ResourceHotReloader.hpp"
#include "../gameplay/SandboxGameplay.hpp"
#include "../save/SaveManager.hpp"

namespace sandbox::tooling {

namespace {

std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tmLocal{};
#if defined(_WIN32)
    localtime_s(&tmLocal, &time);
#else
    localtime_r(&time, &tmLocal);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmLocal, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

void ToolingOverlay::AddNotification(const std::string& message) {
    m_notifications.emplace_back(std::chrono::system_clock::now(), message);
    const std::size_t kMaxNotifications = 10;
    if (m_notifications.size() > kMaxNotifications) {
        m_notifications.erase(m_notifications.begin(),
                              m_notifications.begin() + (m_notifications.size() - kMaxNotifications));
    }
}

void ToolingOverlay::Render(bool& overlayOpen) {
    PruneNotifications();

    if (!overlayOpen) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("Sandbox Tooling", &overlayOpen, windowFlags)) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Quick Save (F5)") && m_callbacks.quickSave) {
            m_callbacks.quickSave();
        }
        ImGui::SameLine();
        if (ImGui::Button("Quick Load (F9)") && m_callbacks.quickLoad) {
            m_callbacks.quickLoad();
        }

        if (m_callbacks.reloadResources) {
            if (ImGui::Button("Reload Resources")) {
                m_callbacks.reloadResources();
            }
        }
    }

    if (ImGui::CollapsingHeader("Hot Reload", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_hotReloader) {
            bool enabled = m_hotReloader->IsEnabled();
            if (ImGui::Checkbox("Enabled", &enabled)) {
                m_hotReloader->SetEnabled(enabled);
            }

            double interval = m_hotReloader->GetPollInterval();
            if (ImGui::DragScalar("Poll Interval (s)", ImGuiDataType_Double, &interval, 0.05f, nullptr, nullptr, "%.2f")) {
                if (interval < 0.1) interval = 0.1;
                m_hotReloader->SetPollInterval(interval);
            }

            if (ImGui::Button("Force Poll")) {
                m_hotReloader->ForcePoll();
            }
        } else {
            ImGui::TextUnformatted("Hot reloader unavailable.");
        }
    }

    if (ImGui::CollapsingHeader("Save Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Refresh Save List")) {
            RefreshSaveList();
        }
        ImGui::SameLine();
        if (m_lastSaveRefresh.time_since_epoch().count() != 0) {
            ImGui::Text("Last refresh: %s", FormatTimestamp(m_lastSaveRefresh).c_str());
        }

        RenderSaveTable();
    }

    if (ImGui::CollapsingHeader("World Snapshot", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_gameplay) {
            ImGui::Text("Scene: %s", m_gameplay->GetActiveSceneName().c_str());
            ImGui::Text("World Time: %.2fs", m_gameplay->GetWorldTimeSeconds());
        }

        if (m_camera) {
            auto pos = m_camera->Position();
            auto forward = m_camera->Front();
            ImGui::Text("Camera Pos:  %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
            ImGui::Text("Camera Dir:  %.2f, %.2f, %.2f", forward.x, forward.y, forward.z);
        }

        if (auto scene = m_scene.lock()) {
            ImGui::Text("GameObjects: %zu", scene->GetAllGameObjects().size());
        }
    }

    RenderNotifications();

    ImGui::End();
}

void ToolingOverlay::RenderSaveTable() {
    if (m_cachedSaves.empty()) {
        ImGui::TextUnformatted("No saves found.");
        return;
    }

    if (ImGui::BeginTable("SavesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Slot");
        ImGui::TableSetupColumn("Modified");
        ImGui::TableSetupColumn("Size (KB)");
        ImGui::TableHeadersRow();

        for (const auto& meta : m_cachedSaves) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(meta.slotName.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", FormatTimestamp(meta.timestamp).c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", meta.fileSizeBytes / 1024.0);
        }

        ImGui::EndTable();
    }
}

void ToolingOverlay::RenderNotifications() {
    if (m_notifications.empty()) {
        return;
    }

    if (ImGui::CollapsingHeader("Notifications", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto now = std::chrono::system_clock::now();
        for (const auto& [timestamp, message] : m_notifications) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();
            ImGui::BulletText("[%llds] %s", static_cast<long long>(age), message.c_str());
        }
    }
}

void ToolingOverlay::RefreshSaveList() {
    if (!m_saveManager) {
        m_cachedSaves.clear();
        return;
    }
    m_cachedSaves = m_saveManager->EnumerateSaves();
    m_lastSaveRefresh = std::chrono::system_clock::now();
}

void ToolingOverlay::PruneNotifications() {
    if (m_notifications.empty()) return;

    auto now = std::chrono::system_clock::now();
    const auto maxAge = std::chrono::seconds(20);
    m_notifications.erase(
        std::remove_if(m_notifications.begin(), m_notifications.end(),
                       [&](const auto& entry) { return now - entry.first > maxAge; }),
        m_notifications.end());
}

} // namespace sandbox::tooling

