#pragma once
#include "gm/scene/Component.hpp"
#include <glm/glm.hpp>
#include <string>

namespace gm {
class Shader;
class Texture;
class Mesh;
class Camera;
struct Transform;
}

/**
 * @brief Simple renderer that spins a mesh using shared resources.
 *
 * This component drives a renderable mesh with optional material data and
 * applies a constant Y-rotation over time. Asset references are stored as
 * paths so scenes can be serialized and rehydrated by the sandbox.
 */
class MeshSpinnerComponent : public gm::Component {
private:
    gm::Mesh* mesh = nullptr;  // Non-owning pointer
    gm::Texture* texture = nullptr;  // Non-owning pointer
    gm::Shader* shader = nullptr;  // Non-owning pointer to the shared shader
    gm::Camera* camera = nullptr;  // Non-owning pointer to the camera
    glm::mat4 projMatrix = glm::mat4(1.0f);  // Projection matrix set per frame
    
    float rotationSpeed = 20.0f;  // Degrees per second
    
    // Asset paths for serialization
    std::string meshPath;
    std::string texturePath;
    std::string shaderVertPath;
    std::string shaderFragPath;

public:
    MeshSpinnerComponent();
    virtual ~MeshSpinnerComponent() = default;

    void Init() override;
    void Update(float deltaTime) override;
    void Render() override;

    // Setters for dependencies
    void SetMesh(gm::Mesh* m) { mesh = m; }
    void SetTexture(gm::Texture* t) { texture = t; }
    void SetShader(gm::Shader* s) { shader = s; }
    void SetCamera(gm::Camera* c) { camera = c; }
    void SetProjectionMatrix(const glm::mat4& proj) { projMatrix = proj; }
    void SetRotationSpeed(float speed) { rotationSpeed = speed; }
    
    // Asset path setters (for serialization)
    void SetMeshPath(const std::string& path) { meshPath = path; }
    void SetTexturePath(const std::string& path) { texturePath = path; }
    void SetShaderPaths(const std::string& vertPath, const std::string& fragPath) {
        shaderVertPath = vertPath;
        shaderFragPath = fragPath;
    }

    // Getters
    gm::Mesh* GetMesh() const { return mesh; }
    gm::Texture* GetTexture() const { return texture; }
    gm::Shader* GetShader() const { return shader; }
    gm::Camera* GetCamera() const { return camera; }
    float GetRotationSpeed() const { return rotationSpeed; }
    
    // Asset path getters (for serialization)
    const std::string& GetMeshPath() const { return meshPath; }
    const std::string& GetTexturePath() const { return texturePath; }
    const std::string& GetShaderVertPath() const { return shaderVertPath; }
    const std::string& GetShaderFragPath() const { return shaderFragPath; }
};
