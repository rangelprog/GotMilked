#if GM_DEBUG_TOOLS

#include "../DebugMenu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/format.h>

namespace {
constexpr int kCurveSamples = 128;

using TimelineKeyframe = gm::debug::DebugMenu::TimeOfDayTimelineKeyframe;
using TimelineState = gm::debug::DebugMenu::TimeOfDayTimelineState;

void EnsureTimelineDefaults(TimelineState& state) {
    if (state.keyframes.size() < 2) {
        state.keyframes.clear();
        state.keyframes.push_back({0.0f, 0.0f});
        state.keyframes.push_back({state.durationSeconds, 1.0f});
        state.needsSort = false;
    }
}

void SortTimeline(TimelineState& state) {
    if (!state.needsSort) {
        return;
    }
    std::sort(state.keyframes.begin(), state.keyframes.end(), [](const TimelineKeyframe& a, const TimelineKeyframe& b) {
        return a.timeSeconds < b.timeSeconds;
    });
    state.needsSort = false;
}

float EvaluateTimeline(const TimelineState& state, float cursorSeconds) {
    if (state.keyframes.empty() || state.durationSeconds <= 0.0f) {
        float normalized = std::clamp(cursorSeconds / std::max(0.001f, state.durationSeconds), 0.0f, 1.0f);
        return normalized;
    }

    const auto& keys = state.keyframes;
    if (cursorSeconds <= keys.front().timeSeconds) {
        return keys.front().normalizedValue;
    }
    if (cursorSeconds >= keys.back().timeSeconds) {
        return keys.back().normalizedValue;
    }

    for (size_t i = 1; i < keys.size(); ++i) {
        if (cursorSeconds <= keys[i].timeSeconds) {
            const auto& prev = keys[i - 1];
            const auto& next = keys[i];
            float denom = std::max(0.0001f, next.timeSeconds - prev.timeSeconds);
            float t = std::clamp((cursorSeconds - prev.timeSeconds) / denom, 0.0f, 1.0f);
#if __cpp_lib_interpolate
            return std::lerp(prev.normalizedValue, next.normalizedValue, t);
#else
            return prev.normalizedValue + (next.normalizedValue - prev.normalizedValue) * t;
#endif
        }
    }
    return keys.back().normalizedValue;
}

} // namespace

namespace gm::debug {

void DebugMenu::RenderCelestialDebugger() {
    if (!ImGui::Begin("Celestial Debugger", &m_showCelestialDebugger)) {
        ImGui::End();
        return;
    }

    if (!m_callbacks.getCelestialConfig ||
        !m_callbacks.getSunMoonState ||
        !m_callbacks.getTimeOfDayNormalized) {
        ImGui::TextWrapped("Celestial callbacks are not connected.");
        ImGui::End();
        return;
    }

    gm::scene::CelestialConfig config = m_callbacks.getCelestialConfig();
    gm::scene::SunMoonState state = m_callbacks.getSunMoonState();
    float normalizedTime = m_callbacks.getTimeOfDayNormalized();

    if (ImGui::CollapsingHeader("Time Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        float hours = normalizedTime * 24.0f;
        if (ImGui::SliderFloat("Time of Day", &normalizedTime, 0.0f, 1.0f, "%.3f")) {
            if (m_callbacks.setTimeOfDayNormalized) {
                m_callbacks.setTimeOfDayNormalized(normalizedTime);
            }
        }
        ImGui::Text("Hours: %.2f", hours);
        ImGui::SameLine();
        if (ImGui::Button("Dawn")) {
            normalizedTime = 0.25f;
            if (m_callbacks.setTimeOfDayNormalized) {
                m_callbacks.setTimeOfDayNormalized(normalizedTime);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Noon")) {
            normalizedTime = 0.5f;
            if (m_callbacks.setTimeOfDayNormalized) {
                m_callbacks.setTimeOfDayNormalized(normalizedTime);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Dusk")) {
            normalizedTime = 0.75f;
            if (m_callbacks.setTimeOfDayNormalized) {
                m_callbacks.setTimeOfDayNormalized(normalizedTime);
            }
        }
    }

    normalizedTime = RenderTimeOfDayTimeline(normalizedTime);

    if (ImGui::CollapsingHeader("Celestial Config", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool dirty = false;
        dirty |= ImGui::SliderFloat("Latitude (deg)", &config.latitudeDeg, -89.0f, 89.0f);
        dirty |= ImGui::SliderFloat("Axial Tilt (deg)", &config.axialTiltDeg, -45.0f, 45.0f);
        dirty |= ImGui::SliderFloat("Day Length (s)", &config.dayLengthSeconds, 60.0f, 7200.0f);
        dirty |= ImGui::DragFloat("Time Offset (s)", &config.timeOffsetSeconds, 1.0f);
        dirty |= ImGui::DragFloat("Moon Offset (s)", &config.moonPhaseOffsetSeconds, 1.0f);
        dirty |= ImGui::SliderFloat("Moonlight Intensity", &config.moonlightIntensity, 0.0f, 1.0f);
        dirty |= ImGui::SliderFloat("Turbidity", &config.turbidity, 1.0f, 10.0f);
        dirty |= ImGui::SliderFloat("Exposure", &config.exposure, 0.1f, 4.0f);
        dirty |= ImGui::SliderFloat("Air Density", &config.airDensity, 0.1f, 2.5f);
        dirty |= ImGui::ColorEdit3("Ground Albedo", &config.groundAlbedo.x);
        dirty |= ImGui::Checkbox("Use Gradient Sky", &config.useGradientSky);
        dirty |= ImGui::SliderFloat("Midday Lux", &config.middayLux, 1000.0f, 120000.0f, "%.0f");
        dirty |= ImGui::SliderFloat("Exposure Reference Lux", &config.exposureReferenceLux, 500.0f, 5000.0f, "%.0f");
        dirty |= ImGui::SliderFloat("Exposure Target EV", &config.exposureTargetEv, 5.0f, 14.0f);
        dirty |= ImGui::SliderFloat("Exposure Bias", &config.exposureBias, 0.25f, 4.0f);
        dirty |= ImGui::SliderFloat("Exposure Smoothing", &config.exposureSmoothing, 0.0f, 0.99f);
        dirty |= ImGui::SliderFloat("Exposure Min", &config.exposureMin, 0.05f, 1.0f);
        dirty |= ImGui::SliderFloat("Exposure Max", &config.exposureMax, 1.0f, 8.0f);
        if (dirty && m_callbacks.setCelestialConfig) {
            m_callbacks.setCelestialConfig(config);
        }
    }

    if (ImGui::CollapsingHeader("Curve Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
        std::array<float, kCurveSamples> sunElevation{};
        std::array<float, kCurveSamples> sunIntensity{};
        gm::scene::TimeOfDayController preview;
        preview.SetConfig(config);

        for (int i = 0; i < kCurveSamples; ++i) {
            float t = static_cast<float>(i) / (kCurveSamples - 1);
            preview.SetTimeSeconds(t * std::max(1.0f, config.dayLengthSeconds));
            auto sample = preview.Evaluate();
            sunElevation[i] = sample.sunElevationDeg / 90.0f;
            sunIntensity[i] = sample.sunIntensity;
        }

        ImGui::PlotLines("Sun Elevation (norm)", sunElevation.data(), kCurveSamples, 0,
                         nullptr, -1.0f, 1.0f, ImVec2(0, 120));
        ImGui::PlotLines("Sun Intensity", sunIntensity.data(), kCurveSamples, 0,
                         nullptr, 0.0f, 1.0f, ImVec2(0, 120));
    }

    if (ImGui::CollapsingHeader("Live State", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto formatVec = [](const glm::vec3& v) {
            return fmt::format("({:.2f}, {:.2f}, {:.2f})", v.x, v.y, v.z);
        };
        ImGui::Text("Sun dir: %s", formatVec(state.sunDirection).c_str());
        ImGui::Text("Sun elev: %.2f deg", state.sunElevationDeg);
        ImGui::Text("Sun intensity: %.2f", state.sunIntensity);
        ImGui::Separator();
        ImGui::Text("Moon dir: %s", formatVec(state.moonDirection).c_str());
        ImGui::Text("Moon elev: %.2f deg", state.moonElevationDeg);
        ImGui::Text("Moon intensity: %.2f", state.moonIntensity);
    }

    if (m_callbacks.getWeatherState && m_callbacks.getWeatherProfileNames) {
        WeatherState weatherState = m_callbacks.getWeatherState();
        auto profileNames = m_callbacks.getWeatherProfileNames();
        if (ImGui::CollapsingHeader("Weather Profiles", ImGuiTreeNodeFlags_DefaultOpen)) {
            int currentIndex = 0;
            for (int i = 0; i < static_cast<int>(profileNames.size()); ++i) {
                if (profileNames[i] == weatherState.activeProfile) {
                    currentIndex = i;
                    break;
                }
            }
            const char* currentLabel = profileNames.empty() ? "n/a" : profileNames[currentIndex].c_str();
            if (ImGui::BeginCombo("Active Profile", currentLabel)) {
                for (int i = 0; i < static_cast<int>(profileNames.size()); ++i) {
                    bool selected = (i == currentIndex);
                    if (ImGui::Selectable(profileNames[i].c_str(), selected)) {
                        currentIndex = i;
                        if (m_callbacks.setWeatherProfile) {
                            m_callbacks.setWeatherProfile(profileNames[i]);
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Text("Wind dir: (%.2f, %.2f, %.2f)",
                        weatherState.windDirection.x,
                        weatherState.windDirection.y,
                        weatherState.windDirection.z);
            ImGui::Text("Wind speed: %.1f m/s", weatherState.windSpeed);
        }
    }

    ImGui::End();
}

float DebugMenu::RenderTimeOfDayTimeline(float normalizedTime) {
    auto& state = m_timeOfDayTimeline;
    constexpr float kMinDuration = 10.0f;
    state.durationSeconds = std::max(state.durationSeconds, kMinDuration);
    state.playbackCursor = std::clamp(state.playbackCursor, 0.0f, state.durationSeconds);

    EnsureTimelineDefaults(state);
    SortTimeline(state);

    auto applyTime = [&](float value) {
        normalizedTime = value;
        if (m_callbacks.setTimeOfDayNormalized) {
            m_callbacks.setTimeOfDayNormalized(value);
        }
    };

    if (state.playing && state.keyframes.size() >= 2) {
        state.playbackCursor += ImGui::GetIO().DeltaTime;
        if (state.playbackCursor > state.durationSeconds) {
            if (state.loop) {
                state.playbackCursor = std::fmod(state.playbackCursor, state.durationSeconds);
            } else {
                state.playbackCursor = state.durationSeconds;
                state.playing = false;
            }
        }
        applyTime(EvaluateTimeline(state, state.playbackCursor));
    }

    if (!ImGui::CollapsingHeader("Time-of-Day Timeline", ImGuiTreeNodeFlags_DefaultOpen)) {
        return normalizedTime;
    }

    ImGui::Checkbox("Play Timeline", &state.playing);
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &state.loop);
    ImGui::SameLine();
    if (ImGui::Button("Sync Cursor From Scene")) {
        state.playbackCursor = std::clamp(normalizedTime, 0.0f, 1.0f) * state.durationSeconds;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Curve")) {
        state.keyframes = {
            TimeOfDayTimelineKeyframe{0.0f, 0.0f},
            TimeOfDayTimelineKeyframe{state.durationSeconds, 1.0f}
        };
        state.needsSort = false;
        SortTimeline(state);
    }

    float previousDuration = state.durationSeconds;
    if (ImGui::DragFloat("Timeline Duration (s)", &state.durationSeconds, 1.0f, kMinDuration, 14400.0f, "%.1f")) {
        state.durationSeconds = std::max(state.durationSeconds, kMinDuration);
        float scale = state.durationSeconds / std::max(previousDuration, 0.001f);
        for (auto& key : state.keyframes) {
            key.timeSeconds = std::clamp(key.timeSeconds * scale, 0.0f, state.durationSeconds);
        }
        state.playbackCursor = std::clamp(state.playbackCursor * scale, 0.0f, state.durationSeconds);
        state.needsSort = true;
    }

    if (ImGui::Button("Add Keyframe At Cursor")) {
        state.keyframes.push_back({state.playbackCursor, normalizedTime});
        state.selectedIndex = static_cast<int>(state.keyframes.size()) - 1;
        state.needsSort = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Keyframe From Scene")) {
        state.keyframes.push_back({std::clamp(normalizedTime, 0.0f, 1.0f) * state.durationSeconds, normalizedTime});
        state.selectedIndex = static_cast<int>(state.keyframes.size()) - 1;
        state.needsSort = true;
    }

    SortTimeline(state);

    constexpr int kTimelineSamples = 256;
    std::array<float, kTimelineSamples> samples{};
    for (int i = 0; i < kTimelineSamples; ++i) {
        float t = (static_cast<float>(i) / (kTimelineSamples - 1)) * state.durationSeconds;
        samples[i] = EvaluateTimeline(state, t);
    }
    ImGui::PlotLines("Timeline Curve", samples.data(), kTimelineSamples, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 120));

    float cursorNormalized = state.durationSeconds <= 0.0f ? 0.0f : state.playbackCursor / state.durationSeconds;
    if (ImGui::SliderFloat("Playback Cursor", &cursorNormalized, 0.0f, 1.0f, "%.3f")) {
        state.playbackCursor = cursorNormalized * state.durationSeconds;
        applyTime(EvaluateTimeline(state, state.playbackCursor));
    }

    if (ImGui::BeginTable("TimelineKeyframes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Time (s)");
        ImGui::TableSetupColumn("Normalized");
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(state.keyframes.size()); ++i) {
            auto& key = state.keyframes[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool selected = (state.selectedIndex == i);
            std::string rowLabel = fmt::format("Key {}", i);
            if (ImGui::Selectable(rowLabel.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
                state.selectedIndex = i;
            }

            ImGui::TableSetColumnIndex(1);
            std::string timeLabel = fmt::format("##time{}", i);
            if (ImGui::DragFloat(timeLabel.c_str(), &key.timeSeconds, 0.1f, 0.0f, state.durationSeconds, "%.2f")) {
                key.timeSeconds = std::clamp(key.timeSeconds, 0.0f, state.durationSeconds);
                state.needsSort = true;
            }

            ImGui::TableSetColumnIndex(2);
            std::string normLabel = fmt::format("##norm{}", i);
            if (ImGui::DragFloat(normLabel.c_str(), &key.normalizedValue, 0.01f, 0.0f, 1.0f, "%.3f")) {
                key.normalizedValue = std::clamp(key.normalizedValue, 0.0f, 1.0f);
            }

            ImGui::TableSetColumnIndex(3);
            bool canDelete = state.keyframes.size() > 2;
            if (!canDelete) {
                ImGui::BeginDisabled();
            }
            std::string deleteLabel = fmt::format("Delete##{}", i);
            if (ImGui::SmallButton(deleteLabel.c_str()) && canDelete) {
                state.keyframes.erase(state.keyframes.begin() + i);
                state.selectedIndex = std::min(state.selectedIndex, static_cast<int>(state.keyframes.size()) - 1);
                state.needsSort = true;
                break;
            }
            if (!canDelete) {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndTable();
    }

    SortTimeline(state);
    return normalizedTime;
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


