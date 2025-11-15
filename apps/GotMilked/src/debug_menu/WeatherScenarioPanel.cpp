#if GM_DEBUG_TOOLS

#include "../DebugMenu.hpp"
#include "../WeatherTypes.hpp"

#include "gm/core/Event.hpp"

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {

template <size_t N>
void CopyStringToBuffer(char (&buffer)[N], const std::string& value) {
    if (value.size() >= N) {
        std::snprintf(buffer, N, "%.*s", static_cast<int>(N - 1), value.c_str());
    } else {
        std::snprintf(buffer, N, "%s", value.c_str());
    }
}

std::string MakeUniqueScenarioName(const std::vector<gm::debug::DebugMenu::WeatherScenario>& scenarios,
                                   std::string base) {
    bool unique = false;
    int suffix = 1;
    while (!unique) {
        unique = std::none_of(scenarios.begin(), scenarios.end(), [&](const auto& scenario) {
            return scenario.name == base;
        });
        if (!unique) {
            base = fmt::format("{} {}", base, suffix++);
        }
    }
    return base;
}

glm::vec3 SafeNormalize(const glm::vec3& v, const glm::vec3& fallback) {
    const float len = glm::length(v);
    if (len < 0.001f) {
        return glm::normalize(fallback);
    }
    return v / len;
}

} // namespace

namespace gm::debug {

void DebugMenu::EnsureWeatherScenarioDefaults() {
    if (!m_weatherScenarios.empty()) {
        return;
    }

    WeatherScenario stormSweep;
    stormSweep.name = "Storm Progression";
    stormSweep.description = "Baseline clear conditions ramp into a storm, then clear up.";
    stormSweep.steps = {
        WeatherScenarioStep{
            .label = "Calm Morning",
            .profile = "default",
            .durationSeconds = 20.0f,
            .wetness = 0.05f,
            .puddles = 0.0f,
            .darkening = 0.05f,
            .windSpeed = 3.5f,
            .windDirection = glm::vec3(0.15f, 0.0f, 0.8f),
            .triggerWeatherEvent = true
        },
        WeatherScenarioStep{
            .label = "Drizzle",
            .profile = "light_rain",
            .durationSeconds = 25.0f,
            .wetness = 0.35f,
            .puddles = 0.15f,
            .darkening = 0.25f,
            .windSpeed = 6.0f,
            .windDirection = glm::vec3(0.1f, 0.0f, 0.9f),
            .triggerWeatherEvent = true,
            .requestLightProbes = true
        },
        WeatherScenarioStep{
            .label = "Heavy Storm",
            .profile = "heavy_rain",
            .durationSeconds = 30.0f,
            .wetness = 0.9f,
            .puddles = 0.7f,
            .darkening = 0.6f,
            .windSpeed = 12.0f,
            .windDirection = glm::vec3(-0.1f, 0.0f, -0.9f),
            .triggerWeatherEvent = true,
            .requestLightProbes = true,
            .requestReflections = true,
            .customEvents = {"fx.rain.intensify"}
        },
        WeatherScenarioStep{
            .label = "Clearing Skies",
            .profile = "default",
            .durationSeconds = 20.0f,
            .wetness = 0.3f,
            .puddles = 0.2f,
            .darkening = 0.25f,
            .windSpeed = 4.5f,
            .windDirection = glm::vec3(-0.2f, 0.0f, 0.6f),
            .triggerWeatherEvent = true,
            .requestReflections = true
        }
    };

    WeatherScenario duskStorm = stormSweep;
    duskStorm.name = "Dusk Fog & Storm";
    duskStorm.description = "Shortened scenario for profiling dusk transitions.";
    duskStorm.steps[0].label = "Golden Hour";
    duskStorm.steps[0].durationSeconds = 15.0f;
    duskStorm.steps[1].label = "Fog Roll-in";
    duskStorm.steps[1].durationSeconds = 18.0f;
    duskStorm.steps[2].label = "Flash Storm";
    duskStorm.steps[2].durationSeconds = 22.0f;
    duskStorm.steps[3].label = "Night Calm";
    duskStorm.steps[3].durationSeconds = 15.0f;

    m_weatherScenarios.push_back(stormSweep);
    m_weatherScenarios.push_back(duskStorm);
    m_selectedWeatherScenario = 0;
}

void DebugMenu::ApplyWeatherScenarioStep(WeatherScenario& scenario, WeatherScenarioStep& step, bool /*fromPlayback*/) {
    WeatherState state = m_callbacks.getWeatherState ? m_callbacks.getWeatherState() : WeatherState{};

    if (!step.profile.empty() && m_callbacks.setWeatherProfile) {
        m_callbacks.setWeatherProfile(step.profile);
        state.activeProfile = step.profile;
    } else if (!step.profile.empty()) {
        state.activeProfile = step.profile;
    }

    state.surfaceWetness = std::clamp(step.wetness, 0.0f, 1.0f);
    state.puddleAmount = std::clamp(step.puddles, 0.0f, 1.0f);
    state.surfaceDarkening = std::clamp(step.darkening, 0.0f, 1.0f);
    state.windSpeed = std::max(0.0f, step.windSpeed);
    state.windDirection = SafeNormalize(step.windDirection, glm::vec3(0.2f, 0.0f, 0.8f));

    if (m_callbacks.setWeatherState) {
        m_callbacks.setWeatherState(state, step.triggerWeatherEvent);
    } else if (step.triggerWeatherEvent && m_callbacks.triggerWeatherEvent) {
        m_callbacks.triggerWeatherEvent();
    }

    if ((step.requestLightProbes || step.requestReflections) && m_callbacks.requestEnvironmentCapture) {
        m_callbacks.requestEnvironmentCapture(step.requestLightProbes, step.requestReflections);
    }

    for (const auto& evt : step.customEvents) {
        if (!evt.empty()) {
            gm::core::Event::Trigger(evt.c_str());
        }
    }

    scenario.stepElapsed = 0.0f;
}

void DebugMenu::AdvanceWeatherScenarioPlayback(WeatherScenario& scenario, float deltaTime) {
    if (!scenario.playbackActive || scenario.steps.empty()) {
        return;
    }

    if (scenario.pendingStepApply) {
        scenario.pendingStepApply = false;
        scenario.currentStep = std::clamp(scenario.currentStep, 0, static_cast<int>(scenario.steps.size()) - 1);
        ApplyWeatherScenarioStep(scenario, scenario.steps[scenario.currentStep], true);
        return;
    }

    scenario.stepElapsed += deltaTime;
    int stepIndex = std::clamp(scenario.currentStep, 0, static_cast<int>(scenario.steps.size()) - 1);
    const float duration = std::max(0.25f, scenario.steps[stepIndex].durationSeconds);
    if (scenario.stepElapsed < duration) {
        return;
    }

    scenario.stepElapsed = 0.0f;
    scenario.currentStep++;
    if (scenario.currentStep >= static_cast<int>(scenario.steps.size())) {
        if (scenario.loopPlayback) {
            scenario.currentStep = 0;
        } else {
            scenario.playbackActive = false;
            scenario.currentStep = static_cast<int>(scenario.steps.size()) - 1;
            return;
        }
    }
    ApplyWeatherScenarioStep(scenario, scenario.steps[scenario.currentStep], true);
}

static void RenderScenarioListControls(std::vector<DebugMenu::WeatherScenario>& scenarios,
                                       int& selectedIndex) {
    if (ImGui::Button("Add Scenario")) {
        DebugMenu::WeatherScenario scenario;
        scenario.name = fmt::format("Scenario {}", scenarios.size() + 1);
        scenario.steps.push_back(DebugMenu::WeatherScenarioStep{});
        scenarios.push_back(scenario);
        selectedIndex = static_cast<int>(scenarios.size()) - 1;
    }
    ImGui::SameLine();
    const bool canRemove = !scenarios.empty();
    ImGui::BeginDisabled(!canRemove);
    if (ImGui::Button("Duplicate") && selectedIndex >= 0 && selectedIndex < static_cast<int>(scenarios.size())) {
        DebugMenu::WeatherScenario clone = scenarios[selectedIndex];
        clone.name = MakeUniqueScenarioName(scenarios, clone.name + " Copy");
        scenarios.insert(scenarios.begin() + selectedIndex + 1, clone);
        selectedIndex++;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove") && selectedIndex >= 0 && selectedIndex < static_cast<int>(scenarios.size())) {
        scenarios.erase(scenarios.begin() + selectedIndex);
        if (scenarios.empty()) {
            selectedIndex = -1;
        } else {
            selectedIndex = std::clamp(selectedIndex, 0, static_cast<int>(scenarios.size()) - 1);
        }
    }
    ImGui::EndDisabled();
}

void DebugMenu::RenderWeatherScenarioEditor() {
    if (!ImGui::Begin("Weather Scenario Editor", &m_showWeatherScenarioEditor)) {
        ImGui::End();
        return;
    }

    EnsureWeatherScenarioDefaults();

    if (m_selectedWeatherScenario >= 0 && m_selectedWeatherScenario < static_cast<int>(m_weatherScenarios.size())) {
        AdvanceWeatherScenarioPlayback(m_weatherScenarios[m_selectedWeatherScenario], ImGui::GetIO().DeltaTime);
    }

    ImGui::BeginChild("ScenarioList", ImVec2(220, 0), true);
    for (int i = 0; i < static_cast<int>(m_weatherScenarios.size()); ++i) {
        bool selected = (i == m_selectedWeatherScenario);
        if (ImGui::Selectable(m_weatherScenarios[i].name.c_str(), selected)) {
            m_selectedWeatherScenario = i;
        }
    }
    RenderScenarioListControls(m_weatherScenarios, m_selectedWeatherScenario);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("ScenarioDetails", ImVec2(0, 0), false);
    if (m_weatherScenarios.empty() || m_selectedWeatherScenario < 0 ||
        m_selectedWeatherScenario >= static_cast<int>(m_weatherScenarios.size())) {
        ImGui::TextUnformatted("Create or select a scenario to edit.");
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::SeparatorText("Test Harness");
    } else {
        auto& scenario = m_weatherScenarios[m_selectedWeatherScenario];
        char nameBuffer[64];
        CopyStringToBuffer(nameBuffer, scenario.name);
        if (ImGui::InputText("Scenario Name", nameBuffer, sizeof(nameBuffer))) {
            scenario.name = nameBuffer;
        }

        char descriptionBuffer[512];
        CopyStringToBuffer(descriptionBuffer, scenario.description);
        if (ImGui::InputTextMultiline("Description", descriptionBuffer, sizeof(descriptionBuffer), ImVec2(-1, 60))) {
            scenario.description = descriptionBuffer;
        }

        ImGui::SeparatorText("Playback");
        ImGui::Checkbox("Loop Scenario", &scenario.loopPlayback);
        ImGui::SameLine();
        ImGui::Text("Current Step: %d/%d", scenario.currentStep + 1, static_cast<int>(scenario.steps.size()));
        ImGui::SameLine();
        if (scenario.playbackActive) {
            if (ImGui::Button("Stop")) {
                scenario.playbackActive = false;
            }
        } else {
            if (ImGui::Button("Play")) {
                scenario.playbackActive = true;
                scenario.currentStep = 0;
                scenario.pendingStepApply = true;
                scenario.stepElapsed = 0.0f;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Step Once") && !scenario.steps.empty()) {
            scenario.currentStep = std::clamp(scenario.currentStep, 0, static_cast<int>(scenario.steps.size()) - 1);
            ApplyWeatherScenarioStep(scenario, scenario.steps[scenario.currentStep], false);
            scenario.currentStep = (scenario.currentStep + 1) % static_cast<int>(scenario.steps.size());
        }

        ImGui::ProgressBar(scenario.steps.empty()
                               ? 0.0f
                               : std::clamp(scenario.stepElapsed /
                                                std::max(0.25f, scenario.steps[scenario.currentStep].durationSeconds),
                                            0.0f, 1.0f),
                           ImVec2(-1, 0));

        ImGui::SeparatorText("Steps");
        auto profileNames = m_callbacks.getWeatherProfileNames ? m_callbacks.getWeatherProfileNames()
                                                               : std::vector<std::string>{};

        for (int i = 0; i < static_cast<int>(scenario.steps.size()); ++i) {
            auto& step = scenario.steps[i];
            std::string header = fmt::format("{}##scenario_step{}", step.label.empty() ? "Step" : step.label, i);
            if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                char labelBuffer[64];
                CopyStringToBuffer(labelBuffer, step.label);
                if (ImGui::InputText(fmt::format("Label##{}", i).c_str(), labelBuffer, sizeof(labelBuffer))) {
                    step.label = labelBuffer;
                }

                char profileBuffer[64];
                CopyStringToBuffer(profileBuffer, step.profile);
                if (ImGui::BeginCombo(fmt::format("Profile##{}", i).c_str(), profileBuffer)) {
                    for (const auto& name : profileNames) {
                        bool selected = (name == step.profile);
                        if (ImGui::Selectable(name.c_str(), selected)) {
                            step.profile = name;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::InputText(fmt::format("Profile Override##{}", i).c_str(), profileBuffer,
                                     sizeof(profileBuffer))) {
                    step.profile = profileBuffer;
                }

                ImGui::DragFloat(fmt::format("Duration (s)##{}", i).c_str(), &step.durationSeconds, 0.25f, 0.25f,
                                 120.0f, "%.2f");
                ImGui::DragFloat(fmt::format("Wetness##{}", i).c_str(), &step.wetness, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(fmt::format("Puddles##{}", i).c_str(), &step.puddles, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(fmt::format("Darkening##{}", i).c_str(), &step.darkening, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat(fmt::format("Wind Speed##{}", i).c_str(), &step.windSpeed, 0.1f, 0.0f, 50.0f);
                ImGui::InputFloat3(fmt::format("Wind Direction##{}", i).c_str(),
                                   glm::value_ptr(step.windDirection));

                ImGui::Checkbox(fmt::format("Broadcast Weather Event##{}", i).c_str(), &step.triggerWeatherEvent);
                ImGui::Checkbox(fmt::format("Request Light Probes##{}", i).c_str(), &step.requestLightProbes);
                ImGui::Checkbox(fmt::format("Request Reflections##{}", i).c_str(), &step.requestReflections);

                if (ImGui::TreeNode(fmt::format("Custom Events##{}", i).c_str())) {
                    for (int evtIndex = 0; evtIndex < static_cast<int>(step.customEvents.size()); ++evtIndex) {
                        auto& evt = step.customEvents[evtIndex];
                        char eventBuffer[96];
                        CopyStringToBuffer(eventBuffer, evt);
                        ImGui::PushID(evtIndex);
                        if (ImGui::InputText("Event", eventBuffer, sizeof(eventBuffer))) {
                            evt = eventBuffer;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Remove")) {
                            step.customEvents.erase(step.customEvents.begin() + evtIndex);
                            ImGui::PopID();
                            break;
                        }
                        ImGui::PopID();
                    }
                    if (ImGui::Button("Add Event")) {
                        step.customEvents.emplace_back();
                    }
                    ImGui::TreePop();
                }

                if (ImGui::Button(fmt::format("Apply Step##{}", i).c_str())) {
                    scenario.currentStep = i;
                    ApplyWeatherScenarioStep(scenario, step, false);
                }
                ImGui::SameLine();
                if (ImGui::Button(fmt::format("Duplicate Step##{}", i).c_str())) {
                    scenario.steps.insert(scenario.steps.begin() + i + 1, step);
                }
                ImGui::SameLine();
                if (ImGui::Button(fmt::format("Delete Step##{}", i).c_str()) && scenario.steps.size() > 1) {
                    scenario.steps.erase(scenario.steps.begin() + i);
                    if (scenario.currentStep >= static_cast<int>(scenario.steps.size())) {
                        scenario.currentStep = std::max(0, static_cast<int>(scenario.steps.size()) - 1);
                    }
                    break;
                }
            }
        }

        if (ImGui::Button("Add Step")) {
            scenario.steps.emplace_back();
        }

        ImGui::EndChild();
        ImGui::Separator();
        ImGui::SeparatorText("Test Harness");
    }

    if (ImGui::Button("Broadcast Weather Event")) {
        if (m_callbacks.triggerWeatherEvent) {
            m_callbacks.triggerWeatherEvent();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Request Captures")) {
        if (m_callbacks.requestEnvironmentCapture) {
            m_callbacks.requestEnvironmentCapture(m_weatherHarness.captureLightProbes,
                                                  m_weatherHarness.captureReflections);
        }
    }
    ImGui::Checkbox("Light Probes", &m_weatherHarness.captureLightProbes);
    ImGui::SameLine();
    ImGui::Checkbox("Reflections", &m_weatherHarness.captureReflections);

    ImGui::InputText("Custom Event", m_weatherHarness.customEvent, sizeof(m_weatherHarness.customEvent));
    ImGui::SameLine();
    if (ImGui::Button("Trigger Custom Event") && m_weatherHarness.customEvent[0] != '\0') {
        gm::core::Event::Trigger(m_weatherHarness.customEvent);
    }

    ImGui::End();
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS

