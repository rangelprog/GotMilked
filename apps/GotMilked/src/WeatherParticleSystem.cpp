#include "WeatherParticleSystem.hpp"

#include "GameResources.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/core/Logger.hpp"

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <random>
#include <algorithm>

namespace {

constexpr const char* kWeatherParticleVert = "shaders/weather_particles.vert.glsl";
constexpr const char* kWeatherParticleFrag = "shaders/weather_particles.frag.glsl";

const WeatherProfile kFallbackProfile{
    .name = "fallback",
    .spawnMultiplier = 0.0f,
    .speedMultiplier = 1.0f,
    .sizeMultiplier = 1.0f,
    .tint = glm::vec3(1.0f)
};

constexpr float kRespawnJitter = 0.35f;

glm::vec3 RandomInBox(std::mt19937& rng, const glm::vec3& extents) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    return glm::vec3(dist(rng) * extents.x,
                     dist(rng) * extents.y,
                     dist(rng) * extents.z);
}

} // namespace

WeatherParticleSystem::WeatherParticleSystem() = default;

WeatherParticleSystem::~WeatherParticleSystem() {
    Shutdown();
}

bool WeatherParticleSystem::Initialize(GameResources& resources) {
    m_resources = &resources;
    m_assetsDir = resources.GetAssetsDirectory();
    Shutdown();

    m_shader = std::make_unique<gm::Shader>();
    const auto vert = (m_assetsDir / kWeatherParticleVert).lexically_normal().string();
    const auto frag = (m_assetsDir / kWeatherParticleFrag).lexically_normal().string();
    if (!m_shader->loadFromFiles(vert, frag)) {
        gm::core::Logger::Error("[WeatherParticleSystem] Failed to load weather particle shader");
        m_shader.reset();
        return false;
    }

    const float quadVertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f
    };

    glGenVertexArrays(1, &m_quadVao);
    glBindVertexArray(m_quadVao);
    glGenBuffers(1, &m_quadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, reinterpret_cast<void*>(0));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void WeatherParticleSystem::Shutdown() {
    if (m_quadVbo != 0) {
        glDeleteBuffers(1, &m_quadVbo);
        m_quadVbo = 0;
    }
    if (m_quadVao != 0) {
        glDeleteVertexArrays(1, &m_quadVao);
        m_quadVao = 0;
    }
    if (m_particleBuffer != 0) {
        glDeleteBuffers(1, &m_particleBuffer);
        m_particleBuffer = 0;
    }
    if (m_metaBuffer != 0) {
        glDeleteBuffers(1, &m_metaBuffer);
        m_metaBuffer = 0;
    }
    if (m_drawCommandBuffer != 0) {
        glDeleteBuffers(1, &m_drawCommandBuffer);
        m_drawCommandBuffer = 0;
    }
    m_shader.reset();
    m_gpuParticles.clear();
    m_gpuMeta.clear();
    m_drawCommands.clear();
    m_emitters.clear();
    m_particleCapacity = 0;
    m_emitterCapacity = 0;
}

void WeatherParticleSystem::Update(gm::Scene& scene,
                                   const WeatherState& state,
                                   const std::unordered_map<std::string, WeatherProfile>& profiles,
                                   float deltaTime) {
    if (!m_resources) {
        return;
    }

    std::unordered_set<const gm::WeatherEmitterComponent*> touched;
    m_emitters.clear();
    float totalSpawnRate = 0.0f;
    for (const auto& object : scene.GetAllGameObjects()) {
        if (!object || !object->IsActive()) {
            continue;
        }

        auto emitter = object->GetComponent<gm::WeatherEmitterComponent>();
        if (!emitter || !emitter->IsActive()) {
            continue;
        }

        auto* emitterPtr = emitter.get();
        auto& runtimeRef = m_runtimeCache[emitterPtr];
        if (!runtimeRef) {
            runtimeRef = std::make_unique<EmitterRuntime>();
            runtimeRef->component = emitterPtr;
            std::random_device rd;
            runtimeRef->rng.seed(rd());
        }
        runtimeRef->component = emitterPtr;
        m_emitters.emplace_back(runtimeRef.get());
        touched.insert(emitterPtr);
    }

    for (auto it = m_runtimeCache.begin(); it != m_runtimeCache.end();) {
        if (!touched.contains(it->first)) {
            it = m_runtimeCache.erase(it);
        } else {
            ++it;
        }
    }

    std::size_t totalNeeded = 0;
    m_diagnostics.emitterCount = m_emitters.size();
    m_diagnostics.particleCapacity = 0;
    for (auto* runtime : m_emitters) {
        if (!runtime || !runtime->component) {
            continue;
        }
        totalSpawnRate += runtime->component->GetSpawnRate();
        totalNeeded += static_cast<std::size_t>(ResolveMaxParticles(*runtime->component));
    }
    EnsureBuffers(totalNeeded, m_emitters.size());

    std::size_t baseInstance = 0;
    m_gpuParticles.clear();
    m_gpuMeta.clear();
    m_drawCommands.clear();

    std::size_t aliveTotal = 0;
    for (auto* runtime : m_emitters) {
        if (!runtime || !runtime->component) {
            continue;
        }
        const WeatherProfile& profile = ResolveProfile(runtime->component->GetProfileTag(), profiles);
        SpawnParticles(*runtime, profile, state, deltaTime);
        UpdateParticles(*runtime, deltaTime);

        EmitterMetaGPU meta{};
        meta.baseInstance = static_cast<std::uint32_t>(baseInstance);
        m_gpuMeta.emplace_back(meta);

        std::uint32_t aliveCount = 0;
        for (auto& particle : runtime->particles) {
            if (!particle.alive) {
                continue;
            }
            ParticleGPU gpu{};
            gpu.posLife = glm::vec4(particle.position, particle.age / particle.lifetime);
            gpu.velSize = glm::vec4(particle.velocity, particle.size);
            gpu.color = glm::vec4(particle.color, 1.0f);
            m_gpuParticles.emplace_back(gpu);
            ++aliveCount;
            ++aliveTotal;
        }

        DrawArraysIndirectCommand cmd{};
        cmd.count = 6u;
        cmd.instanceCount = aliveCount;
        cmd.first = 0u;
        cmd.baseInstance = static_cast<std::uint32_t>(baseInstance);
        m_drawCommands.emplace_back(cmd);
        runtime->baseInstance = cmd.baseInstance;
        baseInstance += aliveCount;
    }

    UploadGpuData();

    m_diagnostics.particleCapacity = m_particleCapacity;
    m_diagnostics.aliveParticles = aliveTotal;
    m_diagnostics.avgSpawnRate = m_emitters.empty() ? 0.0f : totalSpawnRate / static_cast<float>(m_emitters.size());
}

void WeatherParticleSystem::Render(const glm::mat4& view,
                                   const glm::mat4& proj,
                                   const glm::vec3& cameraRight,
                                   const glm::vec3& cameraUp) {
    if (!m_shader || m_drawCommands.empty() || m_gpuParticles.empty()) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    m_shader->Use();
    m_shader->SetMat4("uView", view);
    m_shader->SetMat4("uProj", proj);
    m_shader->SetVec3("uCameraRight", cameraRight);
    m_shader->SetVec3("uCameraUp", cameraUp);

    glBindVertexArray(m_quadVao);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_particleBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_metaBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer);

    glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, static_cast<GLsizei>(m_drawCommands.size()), 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

bool WeatherParticleSystem::EnsureBuffers(std::size_t particleCapacity, std::size_t emitterCount) {
    if (particleCapacity == 0) {
        particleCapacity = 1;
    }
    if (emitterCount == 0) {
        emitterCount = 1;
    }
    m_diagnostics.particleCapacity = particleCapacity;

    if (particleCapacity > m_particleCapacity) {
        m_particleCapacity = particleCapacity;
        if (m_particleBuffer == 0) {
            glGenBuffers(1, &m_particleBuffer);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_particleCapacity * sizeof(ParticleGPU), nullptr, GL_DYNAMIC_DRAW);
    }

    if (emitterCount > m_emitterCapacity) {
        m_emitterCapacity = emitterCount;
        if (m_metaBuffer == 0) {
            glGenBuffers(1, &m_metaBuffer);
        }
        if (m_drawCommandBuffer == 0) {
            glGenBuffers(1, &m_drawCommandBuffer);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_metaBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, m_emitterCapacity * sizeof(EmitterMetaGPU), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, m_emitterCapacity * sizeof(DrawArraysIndirectCommand), nullptr, GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    return true;
}

void WeatherParticleSystem::UploadGpuData() {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_particleBuffer);
    if (!m_gpuParticles.empty()) {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_gpuParticles.size() * sizeof(ParticleGPU), m_gpuParticles.data());
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_metaBuffer);
    if (!m_gpuMeta.empty()) {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_gpuMeta.size() * sizeof(EmitterMetaGPU), m_gpuMeta.data());
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCommandBuffer);
    if (!m_drawCommands.empty()) {
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, m_drawCommands.size() * sizeof(DrawArraysIndirectCommand), m_drawCommands.data());
    }
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void WeatherParticleSystem::SpawnParticles(EmitterRuntime& runtime,
                                           const WeatherProfile& profile,
                                           const WeatherState& state,
                                           float deltaTime) {
    if (!runtime.component) {
        return;
    }

    const float spawnRate = runtime.component->GetSpawnRate() * profile.spawnMultiplier;
    const float maxParticles = ResolveMaxParticles(*runtime.component);

    if (runtime.particles.size() < static_cast<std::size_t>(maxParticles)) {
        runtime.particles.resize(static_cast<std::size_t>(maxParticles));
    }

    runtime.spawnAccumulator += spawnRate * deltaTime;
    const int desiredSpawns = static_cast<int>(std::floor(runtime.spawnAccumulator));
    runtime.spawnAccumulator -= static_cast<float>(desiredSpawns);

    glm::vec3 windDir = state.windDirection;
    if (glm::length2(windDir) < 1e-4f) {
        windDir = glm::vec3(0.0f, 0.0f, 1.0f);
    } else {
        windDir = glm::normalize(windDir);
    }
    const glm::vec3 baseDir = glm::normalize(runtime.component->GetDirection());
    glm::vec3 finalDir = baseDir;
    if (runtime.component->GetAlignToWind()) {
        finalDir = glm::normalize(glm::mix(baseDir, windDir, 0.65f));
    }

    std::uniform_real_distribution<float> jitterDist(0.0f, 1.0f);

    gm::GameObject* owner = runtime.component->GetOwner();
    glm::vec3 basePosition = owner && owner->GetTransform()
                                 ? owner->GetTransform()->GetPosition()
                                 : glm::vec3(0.0f);

    for (int i = 0, spawned = 0; i < desiredSpawns && spawned < desiredSpawns; ++i) {
        auto it = std::find_if(runtime.particles.begin(), runtime.particles.end(), [](const ParticleInstance& p) {
            return !p.alive;
        });
        if (it == runtime.particles.end()) {
            break;
        }

        const glm::vec3 extents = runtime.component->GetVolumeExtents();
        glm::vec3 spawnPos = RandomInBox(runtime.rng, extents) + basePosition;

        ParticleInstance particle;
        particle.position = spawnPos;
        particle.velocity = finalDir * runtime.component->GetParticleSpeed() * profile.speedMultiplier;
        particle.velocity += windDir * (state.windSpeed * 0.15f);
        particle.color = runtime.component->GetBaseColor() * profile.tint;
        particle.size = runtime.component->GetParticleSize() * profile.sizeMultiplier;
        particle.lifetime = runtime.component->GetParticleLifetime();
        particle.age = jitterDist(runtime.rng) * kRespawnJitter;
        particle.alive = true;
        *it = particle;
        ++spawned;
    }
}

void WeatherParticleSystem::UpdateParticles(EmitterRuntime& runtime, float deltaTime) {
    for (auto& particle : runtime.particles) {
        if (!particle.alive) {
            continue;
        }
        particle.age += deltaTime;
        if (particle.age >= particle.lifetime) {
            particle.alive = false;
            continue;
        }
        particle.position += particle.velocity * deltaTime;
    }
}

const WeatherProfile& WeatherParticleSystem::ResolveProfile(const std::string& tag,
                                                            const std::unordered_map<std::string, WeatherProfile>& profiles) const {
    auto it = profiles.find(tag);
    if (it != profiles.end()) {
        return it->second;
    }
    auto fallback = profiles.find("default");
    if (fallback != profiles.end()) {
        return fallback->second;
    }
    return kFallbackProfile;
}

float WeatherParticleSystem::ResolveMaxParticles(const gm::WeatherEmitterComponent& emitter) const {
    switch (m_quality) {
    case WeatherQuality::Low: return emitter.GetMaxParticlesLow();
    case WeatherQuality::Medium: return emitter.GetMaxParticlesMedium();
    case WeatherQuality::High:
    default: return emitter.GetMaxParticlesHigh();
    }
}


