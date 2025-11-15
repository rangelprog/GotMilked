#if GM_DEBUG_TOOLS

#include "../DebugMenu.hpp"
#include "../WeatherParticleSystem.hpp"
#include "../WeatherTypes.hpp"

#include <imgui.h>
#include <fmt/format.h>

namespace gm::debug {

void DebugMenu::RenderWeatherPanel(const WeatherParticleSystem& system) {
    if (!ImGui::Begin("Weather Diagnostics", &m_showWeatherPanel)) {
        ImGui::End();
        return;
    }

    const WeatherParticleSystem::DiagnosticSnapshot stats = system.GetDiagnostics();
    WeatherState state = m_callbacks.getWeatherState ? m_callbacks.getWeatherState() : WeatherState{};

    ImGui::SeparatorText("Weather State");
    ImGui::Text("Active Profile: %s", state.activeProfile.c_str());
    ImGui::Text("Wind Dir: (%.2f, %.2f, %.2f)", state.windDirection.x, state.windDirection.y, state.windDirection.z);
    ImGui::Text("Wind Speed: %.1f m/s", state.windSpeed);
    ImGui::Text("Surface Wetness: %.2f", state.surfaceWetness);
    ImGui::Text("Puddle Amount: %.2f", state.puddleAmount);
    ImGui::Text("Surface Darkening: %.2f", state.surfaceDarkening);

    ImGui::SeparatorText("Emitter Stats");
    ImGui::Text("Emitters: %zu", stats.emitterCount);
    ImGui::Text("Particle Capacity: %zu", stats.particleCapacity);
    ImGui::Text("Alive Particles: %zu", stats.aliveParticles);
    ImGui::Text("Avg Spawn Rate: %.1f/s", stats.avgSpawnRate);

    ImGui::End();
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


