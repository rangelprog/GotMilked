#pragma once

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/vec2.hpp>
#include <filesystem>
#include <memory>
#include <vector>

namespace gm {
class Scene;
class Camera;
class Shader;
class VolumetricFogComponent;
class TransformComponent;
} // namespace gm

namespace gm::scene {
struct SunMoonState;
} // namespace gm::scene

class GameResources;

class VolumetricFogRenderer {
public:
    VolumetricFogRenderer();
    ~VolumetricFogRenderer();

    bool Initialize(GameResources& resources);
    void Shutdown();

    void Render(gm::Scene* scene,
                [[maybe_unused]] const gm::Camera& camera,
                const glm::mat4& view,
                const glm::mat4& proj,
                int fbw,
                int fbh,
                float nearPlane,
                float farPlane,
                const gm::scene::SunMoonState& sunState,
                float timeSeconds);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    void SetQualityLevel(int quality);
    int GetQualityLevel() const { return m_qualityLevel; }

private:
    struct FogVolumeGPU {
        glm::vec4 positionRadius;
        glm::vec4 densityFalloffMaxDistanceEnabled;
        glm::vec4 colorNoiseScale;
        glm::vec4 noiseSpeedPad;
    };

    struct QualityPreset {
        int tileSize = 64;
        int depthSlices = 48;
        float temporalBlend = 0.3f;
    };

    bool EnsureShaders();
    bool EnsureBuffers();
    void EnsureGridResolution(int fbw, int fbh);
    void UploadVolumes(const std::vector<FogVolumeGPU>& volumes);
    void DispatchFroxelPass(std::size_t volumeCount,
                            [[maybe_unused]] const glm::mat4& viewProj,
                            float nearPlane,
                            float farPlane,
                            float timeSeconds);
    void CompositeToScreen([[maybe_unused]] int fbw,
                           [[maybe_unused]] int fbh,
                           const gm::scene::SunMoonState& sunState);
    void UpdateFrustumCache(const glm::mat4& viewProj);
    std::vector<FogVolumeGPU> GatherVolumes(gm::Scene* scene) const;
    void AllocateVolumeBuffer(std::size_t capacity);
    void AllocateFroxelTextures();
    void ClearFroxelTexture() const;
    void UpdateHistoryTexture();
    QualityPreset CurrentPreset() const;

private:
    bool m_enabled = true;
    bool m_initialized = false;
    bool m_historyValid = false;
    int m_qualityLevel = 1;

    glm::ivec3 m_gridSize{32, 18, 48};
    glm::ivec3 m_allocatedGridSize{0, 0, 0};
    glm::vec3 m_frustumCorners[8]{};
    glm::mat4 m_prevViewProj{1.0f};

    std::filesystem::path m_assetsDir;

    GLuint m_volumeBuffer = 0;
    std::size_t m_volumeCapacity = 0;

    GLuint m_froxelImage = 0;
    GLuint m_historyTexture = 0;

    GLuint m_froxelProgram = 0;
    std::unique_ptr<gm::Shader> m_resolveShader;

    GLuint m_screenVao = 0;
    GLuint m_screenVbo = 0;

    float m_temporalBlend = 0.3f;
};


