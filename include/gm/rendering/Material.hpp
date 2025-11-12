#pragma once
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace gm {

class Texture;
class Shader;

/**
 * @brief Material class for managing shader parameters and textures
 * 
 * Encapsulates material properties like diffuse, specular, shininess, etc.
 * Can be shared across multiple objects for efficient rendering.
 */
class Material {
public:
    Material();
    ~Material() = default;

    Material(const Material&) = default;
    Material& operator=(const Material&) = default;
    Material(Material&&) noexcept = default;
    Material& operator=(Material&&) noexcept = default;

    // Apply material properties to a shader
    void Apply(Shader& shader) const;

    // Material properties
    // Diffuse
    void SetDiffuseColor(const glm::vec3& color) { m_diffuseColor = color; m_cacheDirty = true; }
    const glm::vec3& GetDiffuseColor() const { return m_diffuseColor; }
    void SetDiffuseTexture(Texture* texture) { m_diffuseTexture = texture; m_cacheDirty = true; }
    Texture* GetDiffuseTexture() const { return m_diffuseTexture; }
    bool HasDiffuseTexture() const { return m_diffuseTexture != nullptr; }

    // Specular
    void SetSpecularColor(const glm::vec3& color) { m_specularColor = color; m_cacheDirty = true; }
    const glm::vec3& GetSpecularColor() const { return m_specularColor; }
    void SetSpecularTexture(Texture* texture) { m_specularTexture = texture; m_cacheDirty = true; }
    Texture* GetSpecularTexture() const { return m_specularTexture; }
    bool HasSpecularTexture() const { return m_specularTexture != nullptr; }

    // Shininess (specular power)
    void SetShininess(float shininess) { m_shininess = shininess; m_cacheDirty = true; }
    float GetShininess() const { return m_shininess; }

    // Normal map
    void SetNormalTexture(Texture* texture) { m_normalTexture = texture; m_cacheDirty = true; }
    Texture* GetNormalTexture() const { return m_normalTexture; }
    bool HasNormalTexture() const { return m_normalTexture != nullptr; }

    // Emission
    void SetEmissionColor(const glm::vec3& color) { m_emissionColor = color; m_cacheDirty = true; }
    const glm::vec3& GetEmissionColor() const { return m_emissionColor; }
    void SetEmissionTexture(Texture* texture) { m_emissionTexture = texture; m_cacheDirty = true; }
    Texture* GetEmissionTexture() const { return m_emissionTexture; }
    bool HasEmissionTexture() const { return m_emissionTexture != nullptr; }

    // Material name (for debugging/resource management)
    void SetName(const std::string& name) { m_name = name; }
    const std::string& GetName() const { return m_name; }

    // Create common material presets
    static Material CreateDefault();
    static Material CreateUnlit(const glm::vec3& color);
    static Material CreatePhong(const glm::vec3& diffuse, const glm::vec3& specular, float shininess);

private:
    // Diffuse properties
    glm::vec3 m_diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
    Texture* m_diffuseTexture = nullptr;

    // Specular properties
    glm::vec3 m_specularColor = glm::vec3(0.5f, 0.5f, 0.5f);
    Texture* m_specularTexture = nullptr;
    float m_shininess = 32.0f;

    // Normal map
    Texture* m_normalTexture = nullptr;

    // Emission
    glm::vec3 m_emissionColor = glm::vec3(0.0f);
    Texture* m_emissionTexture = nullptr;

    // Metadata
    std::string m_name = "Unnamed Material";
    mutable glm::vec3 m_cachedDiffuse;
    mutable glm::vec3 m_cachedSpecular;
    mutable glm::vec3 m_cachedEmission;
    mutable float m_cachedShininess = 32.0f;
    mutable bool m_cacheDirty = true;
};

} // namespace gm

