#pragma once
#include "gm/scene/Component.hpp"
#include <glm/glm.hpp>

namespace gm {
class Shader;
class Texture;
class Mesh;
class Camera;
struct Transform;
}

/**
 * @brief Component that renders a cow mesh with texture and lighting
 * 
 * Game-specific renderer component for the cow model.
 * This handles:
 * - Mesh and texture rendering
 * - Transform-based positioning and rotation
 * - Lighting setup
 * - Model-view-projection matrix calculations
 */
class CowRendererComponent : public gm::Component {
private:
    gm::Mesh* mesh = nullptr;  // Non-owning pointer
    gm::Texture* texture = nullptr;  // Non-owning pointer
    gm::Shader* shader = nullptr;  // Non-owning pointer to the shared shader
    gm::Camera* camera = nullptr;  // Non-owning pointer to the camera
    glm::mat4 projMatrix = glm::mat4(1.0f);  // Projection matrix set per frame
    
    float rotationSpeed = 20.0f;  // Degrees per second
    float elapsedTime = 0.0f;

public:
    CowRendererComponent();
    virtual ~CowRendererComponent() = default;

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

    // Getters
    gm::Mesh* GetMesh() const { return mesh; }
    gm::Texture* GetTexture() const { return texture; }
    gm::Shader* GetShader() const { return shader; }
    gm::Camera* GetCamera() const { return camera; }
};
