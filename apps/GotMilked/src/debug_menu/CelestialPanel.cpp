#if GM_DEBUG_TOOLS

#include "../DebugMenu.hpp"

#include <array>
#include <fmt/format.h>

namespace {
constexpr int kCurveSamples = 128;
}

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

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


