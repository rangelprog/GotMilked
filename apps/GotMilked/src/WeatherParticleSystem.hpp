#pragma once

#include "gm/scene/WeatherEmitterComponent.hpp"
#include "WeatherTypes.hpp"
#include "gm/scene/Scene.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glad/glad.h>

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <random>

class GameResources;
namespace gm {
class Shader;
}

class WeatherParticleSystem {
public:
    WeatherParticleSystem();
    ~WeatherParticleSystem();

    bool Initialize(GameResources& resources);
    void Shutdown();

    void SetQuality(WeatherQuality quality) { m_quality = quality; }

    void Update(gm::Scene& scene,
                const WeatherState& state,
                const std::unordered_map<std::string, WeatherProfile>& profiles,
                float deltaTime);

    void Render(const glm::mat4& view,
                const glm::mat4& proj,
                const glm::vec3& cameraRight,
                const glm::vec3& cameraUp);

    struct DiagnosticSnapshot {
        std::size_t emitterCount = 0;
        std::size_t particleCapacity = 0;
        std::size_t aliveParticles = 0;
        float avgSpawnRate = 0.0f;
    };

    const DiagnosticSnapshot& GetDiagnostics() const { return m_diagnostics; }

private:
    struct ParticleInstance {
        glm::vec3 position{};
        glm::vec3 velocity{};
        glm::vec3 color{};
        float size = 0.1f;
        float age = 0.0f;
        float lifetime = 1.0f;
        bool alive = false;
    };

    struct EmitterRuntime {
        gm::WeatherEmitterComponent* component = nullptr;
        std::vector<ParticleInstance> particles;
        float spawnAccumulator = 0.0f;
        std::uint32_t baseInstance = 0;
        std::mt19937 rng{1337u};
    };

    struct ParticleGPU {
        glm::vec4 posLife;
        glm::vec4 velSize;
        glm::vec4 color;
    };

    struct DrawArraysIndirectCommand {
        std::uint32_t count = 0;
        std::uint32_t instanceCount = 0;
        std::uint32_t first = 0;
        std::uint32_t baseInstance = 0;
    };

    struct EmitterMetaGPU {
        std::uint32_t baseInstance = 0;
        std::uint32_t padding0 = 0;
        std::uint32_t padding1 = 0;
        std::uint32_t padding2 = 0;
    };

    bool EnsureBuffers(std::size_t particleCapacity, std::size_t emitterCount);
    void UploadGpuData();
    void SpawnParticles(EmitterRuntime& runtime,
                        const WeatherProfile& profile,
                        const WeatherState& state,
                        float deltaTime);
    void UpdateParticles(EmitterRuntime& runtime, float deltaTime);
    const WeatherProfile& ResolveProfile(const std::string& tag,
                                         const std::unordered_map<std::string, WeatherProfile>& profiles) const;
    float ResolveMaxParticles(const gm::WeatherEmitterComponent& emitter) const;

private:
    GameResources* m_resources = nullptr;
    std::filesystem::path m_assetsDir;
    WeatherQuality m_quality = WeatherQuality::High;

    GLuint m_particleBuffer = 0;
    GLuint m_metaBuffer = 0;
    GLuint m_drawCommandBuffer = 0;
    GLuint m_quadVao = 0;
    GLuint m_quadVbo = 0;

    std::unique_ptr<gm::Shader> m_shader;

    std::unordered_map<const gm::WeatherEmitterComponent*, std::unique_ptr<EmitterRuntime>> m_runtimeCache;
    std::vector<EmitterRuntime*> m_emitters;
    std::vector<ParticleGPU> m_gpuParticles;
    std::vector<EmitterMetaGPU> m_gpuMeta;
    std::vector<DrawArraysIndirectCommand> m_drawCommands;

    std::size_t m_particleCapacity = 0;
    std::size_t m_emitterCapacity = 0;
    float m_timeAccumulator = 0.0f;
    DiagnosticSnapshot m_diagnostics;
};


