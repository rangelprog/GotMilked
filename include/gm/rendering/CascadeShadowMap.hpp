#pragma once

#include <vector>
#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace gm::rendering {

struct CascadeShadowSettings {
    struct ElevationBand {
        float minElevationDeg = -90.0f;  // inclusive lower bound
        float splitLambda = 0.8f;
        float stabilizationRadius = 64.0f; // world units
        float depthPadding = 50.0f;        // world units
        float resolutionScale = 1.0f;
    };

    int cascadeCount = 4;
    int baseResolution = 2048;
    float baseSplitLambda = 0.8f;
    float nearPlane = 0.1f;
    float farPlane = 200.0f;
    std::vector<ElevationBand> elevationBands;
};

class CascadeShadowMap {
public:
    CascadeShadowMap();

    void SetSettings(const CascadeShadowSettings& settings);
    const CascadeShadowSettings& GetSettings() const { return m_settings; }

    void Update(const glm::mat4& viewMatrix,
                const glm::mat4& projectionMatrix,
                float cameraNear,
                float cameraFar,
                const glm::vec3& lightDirection,
                float sunElevationDeg);

    const std::vector<glm::mat4>& CascadeMatrices() const { return m_lightMatrices; }
    const std::vector<float>& CascadeSplits() const { return m_cascadeSplits; }
    float ActiveSplitLambda() const { return m_activeSplitLambda; }
    float ActiveStabilizationRadius() const { return m_activeStabilizationRadius; }

private:
    CascadeShadowSettings m_settings;
    std::vector<glm::mat4> m_lightMatrices;
    std::vector<float> m_cascadeSplits;
    float m_activeSplitLambda = 0.8f;
    float m_activeStabilizationRadius = 64.0f;
    float m_activeDepthPadding = 50.0f;

    CascadeShadowSettings::ElevationBand SelectBand(float elevationDeg) const;
    static glm::vec3 NormalizeFallback(const glm::vec3& dir);
};

} // namespace gm::rendering


