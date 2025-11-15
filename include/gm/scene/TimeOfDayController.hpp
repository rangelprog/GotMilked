#pragma once

#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace gm::scene {

struct CelestialConfig {
    float latitudeDeg = 45.0f;          // Observer latitude
    float axialTiltDeg = 23.44f;        // Planet axial tilt
    float dayLengthSeconds = 600.0f;    // Simulated seconds per day
    float timeOffsetSeconds = 0.0f;     // Shift simulation start time
    float moonPhaseOffsetSeconds = 0.0f;
    float moonlightIntensity = 0.25f;
    float turbidity = 2.5f;
    glm::vec3 groundAlbedo = glm::vec3(0.2f, 0.22f, 0.25f);
    float exposure = 1.0f;
    float airDensity = 1.0f;
    bool useGradientSky = false;
    float middayLux = 50000.0f;
    float exposureReferenceLux = 2500.0f;
    float exposureTargetEv = 10.0f;
    float exposureBias = 1.0f;
    float exposureSmoothing = 0.9f;
    float exposureMin = 0.1f;
    float exposureMax = 4.0f;
};

struct SunMoonState {
    glm::vec3 sunDirection{0.0f, 1.0f, 0.0f};
    glm::vec3 moonDirection{0.0f, -1.0f, 0.0f};
    float sunElevationDeg = 0.0f;
    float moonElevationDeg = 0.0f;
    float sunIntensity = 1.0f;
    float moonIntensity = 0.0f;
    float normalizedTime = 0.0f; // [0,1)
    std::array<glm::mat4, 4> sunCascadeMatrices{};
    std::array<float, 4> sunCascadeSplits{};
    int sunCascadeCount = 0;
    glm::mat4 sunViewProjection = glm::mat4(1.0f);
    float sunIlluminanceLux = 0.0f;
    float exposureCompensation = 1.0f;
};

class TimeOfDayController {
public:
    TimeOfDayController();

    void SetConfig(const CelestialConfig& config);
    const CelestialConfig& GetConfig() const { return m_config; }

    void SetTimeSeconds(float seconds);
    void Advance(float deltaSeconds);
    float GetTimeSeconds() const { return m_timeSeconds; }

    float GetNormalizedTime() const;
    SunMoonState Evaluate() const;

private:
    CelestialConfig m_config;
    float m_timeSeconds = 0.0f;

    static glm::vec3 CalculateDirection(float normalizedTime,
                                        float latitudeRad,
                                        float tiltRad);
    static float CalculateElevationDeg(const glm::vec3& dir);
};

} // namespace gm::scene

