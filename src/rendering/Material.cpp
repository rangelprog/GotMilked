#include "gm/rendering/Material.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include <cstdio>

namespace gm {

Material::Material() {
}

void Material::Apply(Shader& shader) const {
    // Diffuse
    if (m_diffuseTexture) {
        shader.SetInt("uUseTex", 1);
        m_diffuseTexture->bind(0);
        shader.SetInt("uTex", 0);
    } else {
        shader.SetInt("uUseTex", 0);
        shader.SetVec3("uSolidColor", m_diffuseColor);
    }

    // Material properties (if shader supports them)
    shader.SetVec3("uMaterial.diffuse", m_diffuseColor);
    shader.SetVec3("uMaterial.specular", m_specularColor);
    shader.SetFloat("uMaterial.shininess", m_shininess);
    shader.SetVec3("uMaterial.emission", m_emissionColor);

    // Texture slots
    int textureSlot = 1;
    
    if (m_specularTexture) {
        m_specularTexture->bind(textureSlot);
        shader.SetInt("uMaterial.specularTex", textureSlot);
        shader.SetInt("uMaterial.useSpecularTex", 1);
        textureSlot++;
    } else {
        shader.SetInt("uMaterial.useSpecularTex", 0);
    }

    if (m_normalTexture) {
        m_normalTexture->bind(textureSlot);
        shader.SetInt("uMaterial.normalTex", textureSlot);
        shader.SetInt("uMaterial.useNormalTex", 1);
        textureSlot++;
    } else {
        shader.SetInt("uMaterial.useNormalTex", 0);
    }

    if (m_emissionTexture) {
        m_emissionTexture->bind(textureSlot);
        shader.SetInt("uMaterial.emissionTex", textureSlot);
        shader.SetInt("uMaterial.useEmissionTex", 1);
        textureSlot++;
    } else {
        shader.SetInt("uMaterial.useEmissionTex", 0);
    }
}

Material Material::CreateDefault() {
    Material mat;
    mat.SetName("Default Material");
    mat.SetDiffuseColor(glm::vec3(0.8f, 0.8f, 0.8f));
    mat.SetSpecularColor(glm::vec3(0.5f, 0.5f, 0.5f));
    mat.SetShininess(32.0f);
    return mat;
}

Material Material::CreateUnlit(const glm::vec3& color) {
    Material mat;
    mat.SetName("Unlit Material");
    mat.SetDiffuseColor(color);
    mat.SetSpecularColor(glm::vec3(0.0f));
    mat.SetShininess(0.0f);
    return mat;
}

Material Material::CreatePhong(const glm::vec3& diffuse, const glm::vec3& specular, float shininess) {
    Material mat;
    mat.SetName("Phong Material");
    mat.SetDiffuseColor(diffuse);
    mat.SetSpecularColor(specular);
    mat.SetShininess(shininess);
    return mat;
}

} // namespace gm

