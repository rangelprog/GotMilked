#include "gm/scene/TimeOfDayController.hpp"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gm::scene {

namespace {
float Wrap01(float value) {
    float wrapped = std::fmod(value, 1.0f);
    if (wrapped < 0.0f) {
        wrapped += 1.0f;
    }
    return wrapped;
}

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

TimeOfDayController::TimeOfDayController() = default;

void TimeOfDayController::SetConfig(const CelestialConfig& config) {
    m_config = config;
}

void TimeOfDayController::SetTimeSeconds(float seconds) {
    m_timeSeconds = seconds;
}

void TimeOfDayController::Advance(float deltaSeconds) {
    m_timeSeconds += deltaSeconds;
}

float TimeOfDayController::GetNormalizedTime() const {
    if (m_config.dayLengthSeconds <= 0.0f) {
        return 0.0f;
    }
    const float scaled = (m_timeSeconds + m_config.timeOffsetSeconds) / m_config.dayLengthSeconds;
    return Wrap01(scaled);
}

SunMoonState TimeOfDayController::Evaluate() const {
    SunMoonState state{};
    const float normalizedTime = GetNormalizedTime();
    const float latitudeRad = glm::radians(m_config.latitudeDeg);
    const float tiltRad = glm::radians(m_config.axialTiltDeg);

    state.normalizedTime = normalizedTime;

    state.sunDirection = CalculateDirection(normalizedTime, latitudeRad, tiltRad);
    state.sunElevationDeg = CalculateElevationDeg(state.sunDirection);

    // Moon trails sun by 180 degrees plus optional offset.
    const float moonTime = Wrap01(normalizedTime + 0.5f +
                                  (m_config.moonPhaseOffsetSeconds / std::max(1.0f, m_config.dayLengthSeconds)));
    state.moonDirection = CalculateDirection(moonTime, latitudeRad, tiltRad);
    state.moonElevationDeg = CalculateElevationDeg(state.moonDirection);

    const float sunAboveHorizon = Clamp01(state.sunDirection.y * 0.5f + 0.5f);
    state.sunIntensity = sunAboveHorizon;
    state.sunIlluminanceLux = sunAboveHorizon * m_config.middayLux;

    const float moonAboveHorizon = Clamp01(state.moonDirection.y * 0.5f + 0.5f);
    state.moonIntensity = moonAboveHorizon * m_config.moonlightIntensity;

    state.sunCascadeCount = 0;
    state.sunViewProjection = glm::mat4(1.0f);
    state.sunCascadeMatrices.fill(glm::mat4(1.0f));
    state.sunCascadeSplits.fill(1.0f);
    state.exposureCompensation = 1.0f;

    return state;
}

glm::vec3 TimeOfDayController::CalculateDirection(float normalizedTime,
                                                  float latitudeRad,
                                                  float tiltRad) {
    const float dayAngle = glm::two_pi<float>() * normalizedTime;

    // Approximate azimuth: rotate around world up. Align 0.25 with sunrise in +X.
    const float azimuth = dayAngle - glm::half_pi<float>();

    // Approximate elevation curve influenced by axial tilt and latitude.
    const float seasonalOffset = tiltRad * std::cos(dayAngle);
    const float latitudeOffset = latitudeRad * 0.5f;
    const float baseElevation = std::sin(dayAngle) * (glm::half_pi<float>() * 0.85f);
    const float elevation = baseElevation + seasonalOffset + latitudeOffset;

    glm::vec3 dir;
    dir.x = std::cos(elevation) * std::cos(azimuth);
    dir.y = std::sin(elevation);
    dir.z = std::cos(elevation) * std::sin(azimuth);
    return glm::normalize(dir);
}

float TimeOfDayController::CalculateElevationDeg(const glm::vec3& dir) {
    return glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
}

} // namespace gm::scene

