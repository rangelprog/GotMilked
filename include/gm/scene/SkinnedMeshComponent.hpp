#pragma once

#include "gm/scene/Component.hpp"
#include "gm/animation/SkinnedMeshAsset.hpp"
#include "gm/utils/ResourceManager.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"

#include <glad/glad.h>
#include <glm/mat4x4.hpp>

#include <memory>
#include <string>
#include <vector>

namespace gm::scene {

class SkinnedMeshComponent : public Component {
public:
    SkinnedMeshComponent();

    static constexpr GLuint kPaletteBindingPoint = 0;
    static constexpr std::size_t kMaxPaletteSize = 128;

    void Init() override;
    void Render() override;
    void OnDestroy() override;

    void SetMesh(std::shared_ptr<animation::SkinnedMeshAsset> mesh, const std::string& guid);
    void SetMesh(ResourceManager::SkinnedMeshHandle handle);

    void SetMaterial(std::shared_ptr<gm::Material> material);
    void SetMaterial(std::shared_ptr<gm::Material> material, const std::string& guid);
    void SetMaterialGuid(const std::string& guid);

    void SetShader(gm::Shader* shader, const std::string& guid);
    void SetShader(ResourceManager::ShaderHandle handle);

    void SetTexture(gm::Texture* texture, const std::string& guid);
    void SetTexture(ResourceManager::TextureHandle handle);

    [[nodiscard]] const std::string& MeshGuid() const { return m_meshGuid; }
    [[nodiscard]] std::shared_ptr<animation::SkinnedMeshAsset> Mesh() const { return m_mesh; }

    [[nodiscard]] gm::Shader* GetShader() const { return m_shader; }
    [[nodiscard]] const std::string& ShaderGuid() const { return m_shaderGuid; }
    [[nodiscard]] std::shared_ptr<gm::Material> GetMaterial() const { return m_material; }
    [[nodiscard]] gm::Texture* GetTexture() const { return m_texture; }
    [[nodiscard]] const std::string& TextureGuid() const { return m_textureGuid; }
    [[nodiscard]] const std::string& MaterialGuid() const { return m_materialGuid; }

    struct GpuMeshEntry;

private:
    void EnsureDefaultMaterial();
    bool EnsureGpuResources() const;
    void RenderMesh() const;
    void RefreshHandles() const;

    mutable std::shared_ptr<animation::SkinnedMeshAsset> m_mesh;
    std::string m_meshGuid;
    mutable ResourceManager::SkinnedMeshHandle m_meshHandle;

    std::shared_ptr<gm::Material> m_material;
    std::string m_materialGuid;
    mutable std::shared_ptr<gm::Shader> m_shaderShared;
    mutable gm::Shader* m_shader = nullptr;
    std::string m_shaderGuid;
    mutable ResourceManager::ShaderHandle m_shaderHandle;

    mutable gm::Texture* m_texture = nullptr;
    std::string m_textureGuid;
    mutable ResourceManager::TextureHandle m_textureHandle;
    mutable std::shared_ptr<gm::Texture> m_textureShared;

    mutable std::vector<glm::mat4> m_paletteMatrices;
    GLuint m_paletteBuffer = 0;

    mutable std::shared_ptr<GpuMeshEntry> m_gpuMesh;
    mutable bool m_gpuMeshDirty = true;
};

} // namespace gm::scene


