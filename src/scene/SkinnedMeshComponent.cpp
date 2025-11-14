#include "gm/scene/SkinnedMeshComponent.hpp"

#include "gm/scene/AnimatorComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/rendering/RenderStateCache.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/utils/ResourceManager.hpp"
#include "gm/core/Logger.hpp"

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace gm::scene {

struct SkinnedMeshComponent::GpuMeshEntry {
    struct Section {
        GLsizei indexCount = 0;
        GLsizei indexOffsetBytes = 0;
        std::string materialGuid;
    };

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    std::vector<Section> sections;

    ~GpuMeshEntry() {
        if (vao != 0) {
            glDeleteVertexArrays(1, &vao);
        }
        if (vbo != 0) {
            glDeleteBuffers(1, &vbo);
        }
        if (ebo != 0) {
            glDeleteBuffers(1, &ebo);
        }
    }
};

namespace {
constexpr std::size_t kMaxBones = SkinnedMeshComponent::kMaxPaletteSize;

struct PackedVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    glm::vec2 uv{0.0f};
    glm::vec4 paintWeights{0.0f};
    std::array<std::uint16_t, 4> boneIndices{0, 0, 0, 0};
    std::array<float, 4> boneWeights{0.0f, 0.0f, 0.0f, 0.0f};
};

std::mutex g_gpuCacheMutex;
std::unordered_map<std::string, std::weak_ptr<SkinnedMeshComponent::GpuMeshEntry>> g_gpuCache;

bool BuildGpuBuffers(const animation::SkinnedMeshAsset& asset,
                     SkinnedMeshComponent::GpuMeshEntry& entry) {
    if (asset.vertices.empty() || asset.indices.empty()) {
        gm::core::Logger::Warning("[SkinnedMeshComponent] Asset '{}' missing geometry", asset.name);
        return false;
    }

    std::vector<PackedVertex> vertices;
    vertices.reserve(asset.vertices.size());
    for (const auto& vertex : asset.vertices) {
        PackedVertex packed;
        packed.position = vertex.position;
        packed.normal = vertex.normal;
        packed.uv = vertex.uv0;
        packed.paintWeights = glm::vec4(0.0f);
        packed.boneIndices = vertex.boneIndices;
        packed.boneWeights = vertex.boneWeights;
        vertices.push_back(packed);
    }

    glGenVertexArrays(1, &entry.vao);
    glGenBuffers(1, &entry.vbo);
    glGenBuffers(1, &entry.ebo);

    glBindVertexArray(entry.vao);

    glBindBuffer(GL_ARRAY_BUFFER, entry.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(PackedVertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(asset.indices.size() * sizeof(std::uint32_t)),
                 asset.indices.data(),
                 GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(PackedVertex));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(PackedVertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(PackedVertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(PackedVertex, uv)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(PackedVertex, paintWeights)));

    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_SHORT, stride,
                           reinterpret_cast<void*>(offsetof(PackedVertex, boneIndices)));

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(PackedVertex, boneWeights)));

    glBindVertexArray(0);

    entry.indexCount = static_cast<GLsizei>(asset.indices.size());
    entry.sections.clear();
    if (!asset.sections.empty()) {
        entry.sections.reserve(asset.sections.size());
        for (const auto& section : asset.sections) {
            if (section.indexCount == 0) {
                continue;
            }
            SkinnedMeshComponent::GpuMeshEntry::Section drawSection;
            drawSection.indexCount = static_cast<GLsizei>(section.indexCount);
            drawSection.indexOffsetBytes =
                static_cast<GLsizei>(section.indexOffset * sizeof(std::uint32_t));
            drawSection.materialGuid = section.materialGuid;
            entry.sections.push_back(drawSection);
        }
    }

    if (entry.sections.empty()) {
        SkinnedMeshComponent::GpuMeshEntry::Section fallback;
        fallback.indexCount = entry.indexCount;
        fallback.indexOffsetBytes = 0;
        entry.sections.push_back(fallback);
    }

    return true;
}

std::shared_ptr<SkinnedMeshComponent::GpuMeshEntry> AcquireGpuMeshEntry(
    const std::shared_ptr<animation::SkinnedMeshAsset>& asset,
    const std::string& guid) {
    if (!asset) {
        return nullptr;
    }

    const std::string cacheKey =
        guid.empty() ? std::string("skinned_") + std::to_string(reinterpret_cast<std::uintptr_t>(asset.get()))
                     : guid;

    {
        std::lock_guard<std::mutex> lock(g_gpuCacheMutex);
        auto it = g_gpuCache.find(cacheKey);
        if (it != g_gpuCache.end()) {
            if (auto existing = it->second.lock()) {
                return existing;
            }
        }
    }

    auto entry = std::make_shared<SkinnedMeshComponent::GpuMeshEntry>();
    if (!BuildGpuBuffers(*asset, *entry)) {
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(g_gpuCacheMutex);
        g_gpuCache[cacheKey] = entry;
    }

    return entry;
}

} // namespace

SkinnedMeshComponent::SkinnedMeshComponent() {
    SetName("SkinnedMeshComponent");
}

void SkinnedMeshComponent::Init() {
    EnsureDefaultMaterial();
    if (m_paletteBuffer == 0) {
        glGenBuffers(1, &m_paletteBuffer);
    }
}

void SkinnedMeshComponent::Render() {
    RefreshHandles();

    if (!m_mesh || !m_shader || !GetOwner()) {
        return;
    }

    auto transform = GetOwner()->GetTransform();
    if (!transform) {
        return;
    }

    const glm::mat4 model = transform->GetMatrix();
    const glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

    bool hasPalette = false;
    if (auto animator = GetOwner()->GetComponent<AnimatorComponent>()) {
        hasPalette = animator->GetSkinningPalette(m_paletteMatrices);
    }

    if (!hasPalette || m_paletteMatrices.empty()) {
        return;
    }

    if (m_paletteMatrices.size() > kMaxBones) {
        gm::core::Logger::Warning(
            "[SkinnedMeshComponent] Bone count {} exceeds maximum {}; truncating palette",
            m_paletteMatrices.size(),
            kMaxBones);
        m_paletteMatrices.resize(kMaxBones);
    }

    if (m_paletteBuffer == 0) {
        glGenBuffers(1, &m_paletteBuffer);
    }

    const GLsizeiptr bufferSize = static_cast<GLsizeiptr>(m_paletteMatrices.size() * sizeof(glm::mat4));
    glBindBuffer(GL_UNIFORM_BUFFER, m_paletteBuffer);
    glBufferData(GL_UNIFORM_BUFFER, bufferSize, m_paletteMatrices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    RenderStateCache::BindUniformBuffer(m_paletteBuffer, kPaletteBindingPoint);

    m_shader->Use();
    m_shader->SetMat4("uModel", model);
    m_shader->SetMat3("uNormalMat", normalMat);

    if (m_material) {
        m_material->Apply(*m_shader);
    }
    if (m_textureShared) {
        RenderStateCache::BindTexture(GL_TEXTURE_2D, m_textureShared->id(), 0);
    } else if (m_texture) {
        RenderStateCache::BindTexture(GL_TEXTURE_2D, m_texture->id(), 0);
    }

    RenderMesh();
}

void SkinnedMeshComponent::OnDestroy() {
    m_mesh.reset();
    m_material.reset();
    m_materialGuid.clear();
    m_shader = nullptr;
    m_shaderShared.reset();
    m_texture = nullptr;
    m_textureShared.reset();
    m_meshHandle.Reset();
    m_shaderHandle.Reset();
    m_textureHandle.Reset();
    m_paletteMatrices.clear();
    if (m_paletteBuffer != 0) {
        RenderStateCache::InvalidateUniformBuffer(m_paletteBuffer);
        glDeleteBuffers(1, &m_paletteBuffer);
        m_paletteBuffer = 0;
    }
    m_gpuMesh.reset();
    m_gpuMeshDirty = true;
}

void SkinnedMeshComponent::SetMesh(std::shared_ptr<animation::SkinnedMeshAsset> mesh, const std::string& guid) {
    m_mesh = std::move(mesh);
    m_meshGuid = guid;
    m_meshHandle.Reset();
    m_gpuMesh.reset();
    m_gpuMeshDirty = true;
}

void SkinnedMeshComponent::SetMesh(ResourceManager::SkinnedMeshHandle handle) {
    m_meshHandle = std::move(handle);
    m_meshGuid = m_meshHandle.Guid();
    m_mesh = m_meshHandle.Lock();
    m_gpuMesh.reset();
    m_gpuMeshDirty = true;
}

void SkinnedMeshComponent::SetMaterial(std::shared_ptr<gm::Material> material) {
    m_material = std::move(material);
    m_materialGuid.clear();
}

void SkinnedMeshComponent::SetMaterial(std::shared_ptr<gm::Material> material, const std::string& guid) {
    m_material = std::move(material);
    m_materialGuid = guid;
}

void SkinnedMeshComponent::SetMaterialGuid(const std::string& guid) {
    m_materialGuid = guid;
}

void SkinnedMeshComponent::SetShader(gm::Shader* shader, const std::string& guid) {
    m_shader = shader;
    m_shaderGuid = guid;
    m_shaderHandle.Reset();
    m_shaderShared.reset();
}

void SkinnedMeshComponent::SetShader(ResourceManager::ShaderHandle handle) {
    m_shaderHandle = std::move(handle);
    m_shaderGuid = m_shaderHandle.Guid();
    m_shaderShared = m_shaderHandle.Lock();
    m_shader = m_shaderShared.get();
}

void SkinnedMeshComponent::SetTexture(gm::Texture* texture, const std::string& guid) {
    m_texture = texture;
    m_textureGuid = guid;
    m_textureHandle.Reset();
    m_textureShared.reset();
}

void SkinnedMeshComponent::SetTexture(ResourceManager::TextureHandle handle) {
    m_textureHandle = std::move(handle);
    m_textureGuid = m_textureHandle.Guid();
    m_textureShared = m_textureHandle.Lock();
    if (m_textureShared) {
        m_texture = m_textureShared.get();
    }
}

void SkinnedMeshComponent::EnsureDefaultMaterial() {
    if (m_material) {
        return;
    }
    m_material = std::make_shared<gm::Material>();
}

bool SkinnedMeshComponent::EnsureGpuResources() const {
    if (!m_mesh && m_meshHandle.IsValid()) {
        m_mesh = m_meshHandle.Lock();
    }

    if (!m_mesh) {
        return false;
    }

    if (!m_gpuMeshDirty && m_gpuMesh) {
        return true;
    }

    m_gpuMesh = AcquireGpuMeshEntry(m_mesh, m_meshGuid);
    if (!m_gpuMesh) {
        m_gpuMeshDirty = true;
        return false;
    }

    m_gpuMeshDirty = false;
    return true;
}

void SkinnedMeshComponent::RenderMesh() const {
    if (!EnsureGpuResources() || !m_gpuMesh) {
        return;
    }

    glBindVertexArray(m_gpuMesh->vao);

    for (const auto& section : m_gpuMesh->sections) {
        if (section.indexCount <= 0) {
            continue;
        }
        const void* offsetPtr =
            reinterpret_cast<const void*>(static_cast<std::uintptr_t>(section.indexOffsetBytes));
        glDrawElements(GL_TRIANGLES, section.indexCount, GL_UNSIGNED_INT, offsetPtr);
    }

    glBindVertexArray(0);
}

void SkinnedMeshComponent::RefreshHandles() const {
    if (!m_mesh && m_meshHandle.IsValid()) {
        m_mesh = m_meshHandle.Lock();
    }
    if (m_meshHandle.IsValid()) {
        auto refreshed = m_meshHandle.Lock();
        if (refreshed) {
            m_mesh = refreshed;
        }
    }
    if (m_shaderHandle.IsValid()) {
        auto refreshed = m_shaderHandle.Lock();
        if (refreshed) {
            m_shaderShared = refreshed;
            m_shader = refreshed.get();
        }
    }
    if (m_textureHandle.IsValid()) {
        m_textureShared = m_textureHandle.Lock();
        if (m_textureShared) {
            m_texture = m_textureShared.get();
        }
    }
}

} // namespace gm::scene


