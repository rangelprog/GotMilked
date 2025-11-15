#include "gm/rendering/CascadeShadowMap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace gm::rendering {

namespace {
constexpr std::array<glm::vec3, 8> kNdcCorners = {
    glm::vec3(-1.f, -1.f, -1.f),
    glm::vec3(1.f, -1.f, -1.f),
    glm::vec3(1.f, 1.f, -1.f),
    glm::vec3(-1.f, 1.f, -1.f),
    glm::vec3(-1.f, -1.f, 1.f),
    glm::vec3(1.f, -1.f, 1.f),
    glm::vec3(1.f, 1.f, 1.f),
    glm::vec3(-1.f, 1.f, 1.f),
};
} // namespace

CascadeShadowMap::CascadeShadowMap() {
    SetSettings(m_settings);
}

void CascadeShadowMap::SetSettings(const CascadeShadowSettings& settings) {
    m_settings = settings;
    if (m_settings.cascadeCount <= 0) {
        m_settings.cascadeCount = 4;
    }
    if (m_settings.baseResolution <= 0) {
        m_settings.baseResolution = 1024;
    }
    if (m_settings.elevationBands.empty()) {
        m_settings.elevationBands = {
            CascadeShadowSettings::ElevationBand{45.0f, 0.60f, 32.0f, 25.0f, 1.0f},
            CascadeShadowSettings::ElevationBand{15.0f, 0.75f, 64.0f, 40.0f, 1.0f},
            CascadeShadowSettings::ElevationBand{-10.0f, 0.85f, 96.0f, 60.0f, 0.9f},
            CascadeShadowSettings::ElevationBand{-90.0f, 0.92f, 128.0f, 80.0f, 0.8f},
        };
    }

    m_lightMatrices.assign(m_settings.cascadeCount, glm::mat4(1.0f));
    m_cascadeSplits.assign(m_settings.cascadeCount, 0.0f);
}

CascadeShadowSettings::ElevationBand CascadeShadowMap::SelectBand(float elevationDeg) const {
    CascadeShadowSettings::ElevationBand selected{ -90.0f, m_settings.baseSplitLambda, 64.0f, 50.0f, 1.0f };
    for (const auto& band : m_settings.elevationBands) {
        if (elevationDeg >= band.minElevationDeg) {
            selected = band;
            break;
        }
    }
    return selected;
}

glm::vec3 CascadeShadowMap::NormalizeFallback(const glm::vec3& dir) {
    const float lenSq = glm::dot(dir, dir);
    if (lenSq < 1e-4f) {
        return glm::vec3(0.0f, -1.0f, 0.0f);
    }
    return glm::normalize(dir);
}

void CascadeShadowMap::Update(const glm::mat4& viewMatrix,
                              const glm::mat4& projectionMatrix,
                              float cameraNear,
                              float cameraFar,
                              const glm::vec3& lightDirection,
                              float sunElevationDeg) {
    const CascadeShadowSettings::ElevationBand band = SelectBand(sunElevationDeg);
    m_activeSplitLambda = band.splitLambda;
    m_activeStabilizationRadius = band.stabilizationRadius;
    m_activeDepthPadding = band.depthPadding;

    const glm::mat4 invViewProj = glm::inverse(projectionMatrix * viewMatrix);

    std::array<glm::vec3, 8> frustumCornersWS{};
    for (std::size_t i = 0; i < kNdcCorners.size(); ++i) {
        glm::vec4 corner = invViewProj * glm::vec4(kNdcCorners[i], 1.0f);
        corner /= corner.w;
        frustumCornersWS[i] = glm::vec3(corner);
    }

    const float clipRange = cameraFar - cameraNear;
    const float ratio = cameraFar / cameraNear;

    float prevSplitDist = cameraNear;

    for (int cascadeIndex = 0; cascadeIndex < m_settings.cascadeCount; ++cascadeIndex) {
        const float p = static_cast<float>(cascadeIndex + 1) / static_cast<float>(m_settings.cascadeCount);
        const float logSplit = cameraNear * std::pow(ratio, p);
        const float linearSplit = cameraNear + clipRange * p;
        const float splitDist = glm::mix(linearSplit, logSplit, m_activeSplitLambda);
        m_cascadeSplits[cascadeIndex] = (splitDist - cameraNear) / clipRange;

        const float prevNorm = (prevSplitDist - cameraNear) / clipRange;
        const float splitNorm = (splitDist - cameraNear) / clipRange;

        std::array<glm::vec3, 8> cascadeCorners{};
        for (int i = 0; i < 4; ++i) {
            const glm::vec3 cornerNear = frustumCornersWS[i];
            const glm::vec3 cornerFar = frustumCornersWS[i + 4];
            cascadeCorners[i] = cornerNear + (cornerFar - cornerNear) * prevNorm;
            cascadeCorners[i + 4] = cornerNear + (cornerFar - cornerNear) * splitNorm;
        }

        glm::vec3 cascadeCenter(0.0f);
        for (const auto& corner : cascadeCorners) {
            cascadeCenter += corner;
        }
        cascadeCenter /= static_cast<float>(cascadeCorners.size());

        glm::vec3 lightDir = NormalizeFallback(-lightDirection);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(lightDir, up)) > 0.96f) {
            up = glm::vec3(0.0f, 0.0f, 1.0f);
        }

        glm::mat4 lightView = glm::lookAt(cascadeCenter - lightDir * 100.0f,
                                          cascadeCenter,
                                          up);

        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float minY = std::numeric_limits<float>::max();
        float maxY = std::numeric_limits<float>::lowest();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::lowest();

        for (const auto& corner : cascadeCorners) {
            glm::vec4 tr = lightView * glm::vec4(corner, 1.0f);
            minX = std::min(minX, tr.x);
            maxX = std::max(maxX, tr.x);
            minY = std::min(minY, tr.y);
            maxY = std::max(maxY, tr.y);
            minZ = std::min(minZ, tr.z);
            maxZ = std::max(maxZ, tr.z);
        }

        const float extentX = maxX - minX;
        const float extentY = maxY - minY;
        const float extent = std::max(extentX, extentY);
        const float halfExtent = extent * 0.5f;
        glm::vec3 centerLS(
            (minX + maxX) * 0.5f,
            (minY + maxY) * 0.5f,
            (minZ + maxZ) * 0.5f
        );

        const int effectiveResolution = std::max(1, static_cast<int>(m_settings.baseResolution * band.resolutionScale));
        const float texelSize = extent / static_cast<float>(effectiveResolution);
        if (texelSize > 0.0f) {
            centerLS.x = std::floor(centerLS.x / texelSize) * texelSize;
            centerLS.y = std::floor(centerLS.y / texelSize) * texelSize;
        }

        minX = centerLS.x - halfExtent;
        maxX = centerLS.x + halfExtent;
        minY = centerLS.y - halfExtent;
        maxY = centerLS.y + halfExtent;

        minZ -= m_activeDepthPadding;
        maxZ += m_activeDepthPadding;

        glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
        m_lightMatrices[cascadeIndex] = lightProj * lightView;

        prevSplitDist = splitDist;
    }
}

} // namespace gm::rendering


