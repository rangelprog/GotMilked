#include "gm/tooling/Overlay.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>

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
    while (m_notifications.size() > kMaxNotifications) {
        m_notifications.pop_front();
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
    RenderNarrativeSection();
    RenderWeatherSection();
    RenderProfilingSection();
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

    const auto stats = m_physicsWorld->GetBodyStats();
    const int totalBodies = stats.staticBodies + stats.dynamicBodies;

    ImGui::Text("Bodies: %d total", totalBodies);
    ImGui::Text("  Static: %d", stats.staticBodies);
    ImGui::Text("  Dynamic: %d", stats.dynamicBodies);
    ImGui::Text("    Active: %d", stats.activeDynamicBodies);
    ImGui::Text("    Sleeping: %d", stats.sleepingDynamicBodies);

    const int unaccounted = stats.dynamicBodies - (stats.activeDynamicBodies + stats.sleepingDynamicBodies);
    if (unaccounted > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "    Unaccounted (locked): %d", unaccounted);
    }
}

void Overlay::RenderNarrativeSection() {
    if (!m_narrativeProvider) {
        return;
    }
    if (!ImGui::CollapsingHeader("Narrative Events", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    const auto entries = m_narrativeProvider();
    if (entries.empty()) {
        ImGui::TextUnformatted("No narrative events recorded.");
        return;
    }

    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame;
    if (ImGui::BeginTable("NarrativeTable", 4, flags)) {
        ImGui::TableSetupColumn("When", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Type / ID");
        ImGui::TableSetupColumn("Subject");
        ImGui::TableSetupColumn("Details");
        ImGui::TableHeadersRow();

        for (const auto& entry : entries) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(FormatTimestamp(entry.timestamp).c_str());

            ImGui::TableSetColumnIndex(1);
            const char* typeLabel = entry.type == NarrativeEntry::Type::Quest ? "Quest" : "Dialogue";
            ImGui::Text("%s: %s", typeLabel, entry.identifier.c_str());

            ImGui::TableSetColumnIndex(2);
            if (entry.subject.empty()) {
                ImGui::TextUnformatted("-");
            } else {
                ImGui::TextUnformatted(entry.subject.c_str());
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("Pos: %.1f, %.1f, %.1f", entry.location.x, entry.location.y, entry.location.z);
            if (entry.sceneLoad) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.2f, 1.0f), "[scene load]");
            }
            if (entry.repeatable) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "[repeatable]");
            }
            if (entry.type == NarrativeEntry::Type::Dialogue && entry.autoStart) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "[auto-start]");
            }
        }

        ImGui::EndTable();
    }
}

void Overlay::RenderWeatherSection() {
    if (!m_weatherProvider) {
        return;
    }
    if (!ImGui::CollapsingHeader("Time & Weather", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    auto snapshot = m_weatherProvider();
    if (!snapshot) {
        ImGui::TextUnformatted("Weather service unavailable.");
        return;
    }
    const auto& info = *snapshot;

    const float hours = std::fmod(info.normalizedTime * 24.0f, 24.0f);
    const int hour = static_cast<int>(std::floor(hours));
    const int minutes = static_cast<int>(std::round((hours - hour) * 60.0f));
    ImGui::Text("Local Time: %02d:%02d", hour, minutes);
    if (info.dayLengthSeconds > 0.0f) {
        ImGui::SameLine();
        ImGui::Text("(Day %.0fs)", info.dayLengthSeconds);
    }

    ImGui::Text("Profile: %s", info.activeProfile.c_str());
    ImGui::Text("Wind: %.1f m/s (%.2f, %.2f, %.2f)",
                info.windSpeed,
                info.windDirection.x,
                info.windDirection.y,
                info.windDirection.z);
    ImGui::Text("Surface wetness: %.2f   Puddles: %.2f   Darkening: %.2f",
                info.surfaceWetness,
                info.puddleAmount,
                info.surfaceDarkening);

    if (!info.alerts.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Alerts:");
        for (const auto& alert : info.alerts) {
            ImGui::BulletText("%s", alert.c_str());
        }
    }

    if (!info.forecast.empty()) {
        if (ImGui::BeginTable("ForecastTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Start (hr)");
            ImGui::TableSetupColumn("Profile");
            ImGui::TableSetupColumn("Notes");
            ImGui::TableHeadersRow();
            for (const auto& entry : info.forecast) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%.1f (%.1fh)", entry.startHour, entry.durationHours);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(entry.profile.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(entry.description.c_str());
            }
            ImGui::EndTable();
        }
    }
}

void Overlay::RenderProfilingSection() {
    if (!m_callbacks.applyProfilingPreset) {
        return;
    }
    if (!ImGui::CollapsingHeader("Profiling Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    if (ImGui::Button("Sunny Midday")) {
        m_callbacks.applyProfilingPreset("sunny");
    }
    ImGui::SameLine();
    if (ImGui::Button("Stormy Midday")) {
        m_callbacks.applyProfilingPreset("stormy");
    }
    ImGui::SameLine();
    if (ImGui::Button("Dusk Clear")) {
        m_callbacks.applyProfilingPreset("dusk");
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

