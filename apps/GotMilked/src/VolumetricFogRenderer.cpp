#include "VolumetricFogRenderer.hpp"

#include "GameResources.hpp"
#include "gm/core/Logger.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/VolumetricFogComponent.hpp"
#include "gm/scene/TimeOfDayController.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/common.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace {

constexpr std::size_t kInitialFogCapacity = 32;
constexpr std::size_t kMaxFogCapacity = 512;
constexpr int kMaxGridAxis = 96;

const char* kFroxelShader = "shaders/fog_froxel.comp.glsl";
const char* kResolveVert = "shaders/fog_resolve.vert.glsl";
const char* kResolveFrag = "shaders/fog_resolve.frag.glsl";

bool LoadTextFile(const std::filesystem::path& path, std::string& outSource) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    outSource = ss.str();
    return true;
}

} // namespace

VolumetricFogRenderer::VolumetricFogRenderer() = default;

VolumetricFogRenderer::~VolumetricFogRenderer() {
    Shutdown();
}

void VolumetricFogRenderer::SetQualityLevel(int quality) {
    m_qualityLevel = std::clamp(quality, 0, 2);
    m_temporalBlend = CurrentPreset().temporalBlend;
    m_allocatedGridSize = glm::ivec3(0);
}

bool VolumetricFogRenderer::Initialize(GameResources& resources) {
    if (m_initialized) {
        return true;
    }

    m_assetsDir = resources.GetAssetsDirectory();
    m_temporalBlend = CurrentPreset().temporalBlend;

    if (!EnsureShaders()) {
        gm::core::Logger::Error("[VolumetricFogRenderer] Failed to compile shaders");
        return false;
    }

    if (!EnsureBuffers()) {
        gm::core::Logger::Error("[VolumetricFogRenderer] Failed to allocate screen buffers");
        return false;
    }

    AllocateVolumeBuffer(kInitialFogCapacity);
    AllocateFroxelTextures();
    m_initialized = true;
    return true;
}

void VolumetricFogRenderer::Shutdown() {
    if (m_froxelProgram != 0) {
        glDeleteProgram(m_froxelProgram);
        m_froxelProgram = 0;
    }

    if (m_volumeBuffer != 0) {
        glDeleteBuffers(1, &m_volumeBuffer);
        m_volumeBuffer = 0;
    }

    if (m_froxelImage != 0) {
        glDeleteTextures(1, &m_froxelImage);
        m_froxelImage = 0;
    }

    if (m_historyTexture != 0) {
        glDeleteTextures(1, &m_historyTexture);
        m_historyTexture = 0;
    }

    if (m_screenVbo != 0) {
        glDeleteBuffers(1, &m_screenVbo);
        m_screenVbo = 0;
    }

    if (m_screenVao != 0) {
        glDeleteVertexArrays(1, &m_screenVao);
        m_screenVao = 0;
    }

    m_resolveShader.reset();
    m_volumeCapacity = 0;
    m_allocatedGridSize = glm::ivec3(0);
    m_historyValid = false;
    m_initialized = false;
}

void VolumetricFogRenderer::Render(gm::Scene* scene,
                                   [[maybe_unused]] const gm::Camera& camera,
                                   const glm::mat4& view,
                                   const glm::mat4& proj,
                                   int fbw,
                                   int fbh,
                                   float nearPlane,
                                   float farPlane,
                                   const gm::scene::SunMoonState& sunState,
                                   float timeSeconds) {
    if (!m_enabled || !m_initialized || !scene) {
        return;
    }

    EnsureGridResolution(fbw, fbh);
    auto fogVolumes = GatherVolumes(scene);
    UploadVolumes(fogVolumes);

    const glm::mat4 viewProj = proj * view;
    UpdateFrustumCache(viewProj);

    DispatchFroxelPass(fogVolumes.size(), viewProj, nearPlane, farPlane, timeSeconds);
    CompositeToScreen(fbw, fbh, sunState);

    m_prevViewProj = viewProj;
    m_historyValid = true;
}

VolumetricFogRenderer::QualityPreset VolumetricFogRenderer::CurrentPreset() const {
    switch (m_qualityLevel) {
    case 0:  return QualityPreset{96, 32, 0.45f};
    case 2:  return QualityPreset{48, 64, 0.2f};
    default: return QualityPreset{64, 48, 0.30f};
    }
}

bool VolumetricFogRenderer::EnsureShaders() {
    const auto froxelPath = m_assetsDir / kFroxelShader;
    std::string computeSource;
    if (!LoadTextFile(froxelPath, computeSource)) {
        gm::core::Logger::Error("[VolumetricFogRenderer] Missing compute shader '{}'", froxelPath.string());
        return false;
    }

    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    const char* src = computeSource.c_str();
    glShaderSource(computeShader, 1, &src, nullptr);
    glCompileShader(computeShader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(computeShader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLength = 0;
        glGetShaderiv(computeShader, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetShaderInfoLog(computeShader, logLength, nullptr, log.data());
        gm::core::Logger::Error("[VolumetricFogRenderer] Compute shader compile failed: {}", log);
        glDeleteShader(computeShader);
        return false;
    }

    if (m_froxelProgram != 0) {
        glDeleteProgram(m_froxelProgram);
    }

    m_froxelProgram = glCreateProgram();
    glAttachShader(m_froxelProgram, computeShader);
    glLinkProgram(m_froxelProgram);
    glDeleteShader(computeShader);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_froxelProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLength = 0;
        glGetProgramiv(m_froxelProgram, GL_INFO_LOG_LENGTH, &logLength);
        std::string log(static_cast<std::size_t>(logLength), '\0');
        glGetProgramInfoLog(m_froxelProgram, logLength, nullptr, log.data());
        gm::core::Logger::Error("[VolumetricFogRenderer] Compute program link failed: {}", log);
        glDeleteProgram(m_froxelProgram);
        m_froxelProgram = 0;
        return false;
    }

    m_resolveShader = std::make_unique<gm::Shader>();
    const auto resolveVert = (m_assetsDir / kResolveVert).string();
    const auto resolveFrag = (m_assetsDir / kResolveFrag).string();
    if (!m_resolveShader->loadFromFiles(resolveVert, resolveFrag)) {
        gm::core::Logger::Error("[VolumetricFogRenderer] Failed to load resolve shader '{}'", resolveFrag);
        m_resolveShader.reset();
        return false;
    }

    return true;
}

bool VolumetricFogRenderer::EnsureBuffers() {
    if (m_screenVao != 0) {
        return true;
    }

    const float vertices[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f,
    };

    glGenVertexArrays(1, &m_screenVao);
    glBindVertexArray(m_screenVao);

    glGenBuffers(1, &m_screenVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_screenVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

void VolumetricFogRenderer::EnsureGridResolution(int fbw, int fbh) {
    const auto preset = CurrentPreset();
    const int gridX = std::clamp(fbw / preset.tileSize, 8, kMaxGridAxis);
    const int gridY = std::clamp(fbh / preset.tileSize, 8, kMaxGridAxis);
    const glm::ivec3 desired(gridX, gridY, preset.depthSlices);

    if (desired != m_allocatedGridSize) {
        m_gridSize = desired;
        AllocateFroxelTextures();
    } else {
        m_gridSize = desired;
    }
}

void VolumetricFogRenderer::AllocateVolumeBuffer(std::size_t capacity) {
    capacity = std::clamp(capacity, kInitialFogCapacity, kMaxFogCapacity);
    if (m_volumeBuffer == 0) {
        glGenBuffers(1, &m_volumeBuffer);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_volumeBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, capacity * sizeof(FogVolumeGPU), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    m_volumeCapacity = capacity;
}

void VolumetricFogRenderer::AllocateFroxelTextures() {
    if (m_gridSize.x <= 0 || m_gridSize.y <= 0 || m_gridSize.z <= 0) {
        return;
    }

    if (m_froxelImage == 0) {
        glGenTextures(1, &m_froxelImage);
    }
    glBindTexture(GL_TEXTURE_3D, m_froxelImage);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D,
                 0,
                 GL_RGBA16F,
                 m_gridSize.x,
                 m_gridSize.y,
                 m_gridSize.z,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 nullptr);

    if (m_historyTexture == 0) {
        glGenTextures(1, &m_historyTexture);
    }
    glBindTexture(GL_TEXTURE_3D, m_historyTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D(GL_TEXTURE_3D,
                 0,
                 GL_RGBA16F,
                 m_gridSize.x,
                 m_gridSize.y,
                 m_gridSize.z,
                 0,
                 GL_RGBA,
                 GL_FLOAT,
                 nullptr);
    glBindTexture(GL_TEXTURE_3D, 0);

    m_allocatedGridSize = m_gridSize;
    m_historyValid = false;
    ClearFroxelTexture();
}

void VolumetricFogRenderer::ClearFroxelTexture() const {
    if (m_froxelImage != 0) {
        const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        glClearTexImage(m_froxelImage, 0, GL_RGBA, GL_FLOAT, zero);
    }
}

std::vector<VolumetricFogRenderer::FogVolumeGPU> VolumetricFogRenderer::GatherVolumes(gm::Scene* scene) const {
    std::vector<FogVolumeGPU> result;
    if (!scene) {
        return result;
    }

    auto& objects = scene->GetAllGameObjects();
    result.reserve(objects.size());

    for (const auto& object : objects) {
        if (!object || !object->IsActive()) {
            continue;
        }

        auto fog = object->GetComponent<gm::VolumetricFogComponent>();
        if (!fog || !fog->IsEnabled()) {
            continue;
        }

        auto transform = object->GetComponent<gm::TransformComponent>();
        if (!transform) {
            continue;
        }

        FogVolumeGPU gpu{};
        const glm::vec3 position = transform->GetPosition();
        const glm::vec3 scale = transform->GetScale();
        const float radius = std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z), 0.5f});

        gpu.positionRadius = glm::vec4(position, radius);
        gpu.densityFalloffMaxDistanceEnabled = glm::vec4(
            fog->GetDensity(),
            fog->GetHeightFalloff(),
            fog->GetMaxDistance(),
            1.0f
        );

        const glm::vec3 color = fog->GetColor();
        gpu.colorNoiseScale = glm::vec4(color, std::max(fog->GetNoiseScale(), 0.01f));
        gpu.noiseSpeedPad = glm::vec4(fog->GetNoiseSpeed(), 0.0f, 0.0f, 0.0f);

        result.push_back(gpu);
        if (result.size() >= kMaxFogCapacity) {
            break;
        }
    }

    return result;
}

void VolumetricFogRenderer::UploadVolumes(const std::vector<FogVolumeGPU>& volumes) {
    std::size_t required = std::max<std::size_t>(volumes.size(), 1);
    if (required > m_volumeCapacity) {
        AllocateVolumeBuffer(std::max(required, m_volumeCapacity + kInitialFogCapacity));
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_volumeBuffer);
    if (!volumes.empty()) {
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        0,
                        volumes.size() * sizeof(FogVolumeGPU),
                        volumes.data());
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void VolumetricFogRenderer::UpdateFrustumCache(const glm::mat4& viewProj) {
    const glm::mat4 invViewProj = glm::inverse(viewProj);
    const std::array<glm::vec4, 8> ndcCorners = {{
        {-1.0f, -1.0f, -1.0f, 1.0f},
        { 1.0f, -1.0f, -1.0f, 1.0f},
        { 1.0f,  1.0f, -1.0f, 1.0f},
        {-1.0f,  1.0f, -1.0f, 1.0f},
        {-1.0f, -1.0f,  1.0f, 1.0f},
        { 1.0f, -1.0f,  1.0f, 1.0f},
        { 1.0f,  1.0f,  1.0f, 1.0f},
        {-1.0f,  1.0f,  1.0f, 1.0f},
    }};

    for (int i = 0; i < 8; ++i) {
        glm::vec4 corner = invViewProj * ndcCorners[i];
        corner /= corner.w;
        m_frustumCorners[i] = glm::vec3(corner);
    }
}

void VolumetricFogRenderer::DispatchFroxelPass(std::size_t volumeCount,
                                               [[maybe_unused]] const glm::mat4& viewProj,
                                               float nearPlane,
                                               float farPlane,
                                               float timeSeconds) {
    if (m_froxelProgram == 0 || m_volumeBuffer == 0 || m_froxelImage == 0) {
        return;
    }

    glUseProgram(m_froxelProgram);

    const GLint gridLoc = glGetUniformLocation(m_froxelProgram, "uGridSize");
    glUniform3i(gridLoc, m_gridSize.x, m_gridSize.y, m_gridSize.z);

    glUniform1i(glGetUniformLocation(m_froxelProgram, "uVolumeCount"),
                static_cast<GLint>(volumeCount));
    glUniform1f(glGetUniformLocation(m_froxelProgram, "uNearPlane"), nearPlane);
    glUniform1f(glGetUniformLocation(m_froxelProgram, "uFarPlane"), farPlane);
    glUniform1f(glGetUniformLocation(m_froxelProgram, "uTemporalAlpha"), m_temporalBlend);
    glUniform1f(glGetUniformLocation(m_froxelProgram, "uTime"), timeSeconds);
    glUniform1i(glGetUniformLocation(m_froxelProgram, "uHistoryValid"),
                m_historyValid ? 1 : 0);

    const GLint prevLoc = glGetUniformLocation(m_froxelProgram, "uPrevViewProj");
    glUniformMatrix4fv(prevLoc, 1, GL_FALSE, glm::value_ptr(m_prevViewProj));

    const GLint cornersLoc = glGetUniformLocation(m_froxelProgram, "uFrustumCorners[0]");
    glUniform3fv(cornersLoc, 8, glm::value_ptr(m_frustumCorners[0]));

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_volumeBuffer);
    glBindImageTexture(0, m_froxelImage, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, m_historyTexture);
    glUniform1i(glGetUniformLocation(m_froxelProgram, "uHistoryTexture"), 1);

    const GLuint groupsX = static_cast<GLuint>((m_gridSize.x + 3) / 4);
    const GLuint groupsY = static_cast<GLuint>((m_gridSize.y + 3) / 4);
    const GLuint groupsZ = static_cast<GLuint>((m_gridSize.z + 3) / 4);
    glDispatchCompute(groupsX, groupsY, groupsZ);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                    GL_SHADER_STORAGE_BARRIER_BIT |
                    GL_TEXTURE_FETCH_BARRIER_BIT);

    UpdateHistoryTexture();
}

void VolumetricFogRenderer::UpdateHistoryTexture() {
    if (m_historyTexture == 0 || m_froxelImage == 0) {
        return;
    }

    glCopyImageSubData(m_froxelImage, GL_TEXTURE_3D, 0, 0, 0, 0,
                       m_historyTexture, GL_TEXTURE_3D, 0, 0, 0, 0,
                       m_gridSize.x, m_gridSize.y, m_gridSize.z);
}

void VolumetricFogRenderer::CompositeToScreen([[maybe_unused]] int fbw,
                                              [[maybe_unused]] int fbh,
                                              const gm::scene::SunMoonState& sunState) {
    if (!m_resolveShader || m_screenVao == 0) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    m_resolveShader->Use();
    m_resolveShader->SetInt("uFroxelVolume", 0);
    m_resolveShader->SetInt("uGridDepth", m_gridSize.z);
    m_resolveShader->SetFloat("uIntensityScale", static_cast<float>(m_gridSize.z) / 64.0f);
    const float lux = std::max(sunState.sunIlluminanceLux, 1000.0f);
    const float lightFactor = glm::clamp(lux / 50000.0f, 0.1f, 1.5f);
    m_resolveShader->SetFloat("uLightFactor", lightFactor);
    const GLint cornersLoc = m_resolveShader->uniformLoc("uFrustumCorners[0]");
    glUniform3fv(cornersLoc, 8, glm::value_ptr(m_frustumCorners[0]));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_froxelImage);

    glBindVertexArray(m_screenVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


