#include "gm/tooling/Overlay.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "imgui.h"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/physics/PhysicsWorld.hpp"

namespace gm::tooling {

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

void Overlay::SetSaveManager(gm::save::SaveManager* manager) {
    m_saveManager = manager;
    RefreshSaveList();
}

void Overlay::SetScene(const std::shared_ptr<gm::Scene>& scene) {
    m_scene = scene;
}

void Overlay::AddNotification(const std::string& message) {
    m_notifications.emplace_back(std::chrono::system_clock::now(), message);
    const std::size_t kMaxNotifications = 10;
    if (m_notifications.size() > kMaxNotifications) {
        m_notifications.erase(m_notifications.begin(),
                              m_notifications.begin() + (m_notifications.size() - kMaxNotifications));
    }
}

void Overlay::Render(bool& overlayOpen) {
    PruneNotifications();
    if (!overlayOpen) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin("Tooling", &overlayOpen, windowFlags)) {
        ImGui::End();
        return;
    }

    RenderActionsSection();
    RenderHotReloadSection();
    RenderSaveSection();
    RenderWorldSection();
    RenderPhysicsSection();
    RenderNotifications();

    ImGui::End();
}

void Overlay::RenderActionsSection() {
    if (!ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

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

void Overlay::RenderHotReloadSection() {
    if (!ImGui::CollapsingHeader("Hot Reload", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (!m_hotReloader) {
        ImGui::TextUnformatted("Hot reloader unavailable.");
        return;
    }

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
}

void Overlay::RenderSaveSection() {
    if (!ImGui::CollapsingHeader("Saves", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (ImGui::Button("Refresh Save List")) {
        RefreshSaveList();
    }
    ImGui::SameLine();
    if (m_lastSaveRefresh.time_since_epoch().count() != 0) {
        ImGui::Text("Last refresh: %s", FormatTimestamp(m_lastSaveRefresh).c_str());
    }

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

void Overlay::RenderWorldSection() {
    if (!ImGui::CollapsingHeader("World Snapshot", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (m_worldInfoProvider) {
        if (auto info = m_worldInfoProvider()) {
            ImGui::Text("Scene: %s", info->sceneName.c_str());
            ImGui::Text("World Time: %.2fs", info->worldTimeSeconds);
            ImGui::Text("Camera Pos:  %.2f, %.2f, %.2f",
                        info->cameraPosition.x, info->cameraPosition.y, info->cameraPosition.z);
            ImGui::Text("Camera Dir:  %.2f, %.2f, %.2f",
                        info->cameraDirection.x, info->cameraDirection.y, info->cameraDirection.z);
        }
    }
    else if (m_camera) {
        auto pos = m_camera->Position();
        auto forward = m_camera->Front();
        ImGui::Text("Camera Pos:  %.2f, %.2f, %.2f", pos.x, pos.y, pos.z);
        ImGui::Text("Camera Dir:  %.2f, %.2f, %.2f", forward.x, forward.y, forward.z);
    }

    if (auto scene = m_scene.lock()) {
        ImGui::Text("GameObjects: %zu", scene->GetAllGameObjects().size());
    }
}

void Overlay::RenderPhysicsSection() {
    if (!ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (!m_physicsWorld) {
        ImGui::TextUnformatted("Physics world unavailable.");
        return;
    }

    if (!m_physicsWorld->IsInitialized()) {
        ImGui::TextUnformatted("Physics world not initialized.");
        return;
    }

    ImGui::TextUnformatted("Status: Active");

    // Count physics bodies in the scene
    if (auto scene = m_scene.lock()) {
        int staticBodies = 0;
        int dynamicBodies = 0;
        
        for (const auto& obj : scene->GetAllGameObjects()) {
            if (!obj || !obj->IsActive()) continue;
            
            // Check if object has RigidBodyComponent (we'll need to forward declare or include)
            // For now, we'll use tags as a proxy
            if (obj->HasTag("ground")) {
                staticBodies++;
            } else if (obj->HasTag("dynamic")) {
                dynamicBodies++;
            }
        }
        
        ImGui::Text("Static Bodies: %d", staticBodies);
        ImGui::Text("Dynamic Bodies: %d", dynamicBodies);
    }
}

void Overlay::RenderNotifications() {
    if (m_notifications.empty()) {
        return;
    }

    if (!ImGui::CollapsingHeader("Notifications", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    for (const auto& [timestamp, message] : m_notifications) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - timestamp).count();
        ImGui::BulletText("[%llds] %s", static_cast<long long>(age), message.c_str());
    }
}

void Overlay::RefreshSaveList() {
    if (!m_saveManager) {
        m_cachedSaves.clear();
        return;
    }
    m_cachedSaves = m_saveManager->EnumerateSaves();
    m_lastSaveRefresh = std::chrono::system_clock::now();
}

void Overlay::PruneNotifications() {
    if (m_notifications.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    const auto maxAge = std::chrono::seconds(20);
    m_notifications.erase(
        std::remove_if(m_notifications.begin(), m_notifications.end(),
                       [&](const auto& entry) { return now - entry.first > maxAge; }),
        m_notifications.end());
}

} // namespace gm::tooling

