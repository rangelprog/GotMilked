#pragma once
#include <memory>
#include <vector>
#include "SceneEntity.hpp"

namespace gm {

class Scene {
private:
    std::vector<std::shared_ptr<SceneEntity>> entities;
    bool isInitialized = false;

public:
    Scene() = default;
    ~Scene() = default;

    void Draw(class Shader& shader, const class Camera& cam, int fbw, int fbh, float fovDeg);
    
    // Entity management
    void AddEntity(std::shared_ptr<SceneEntity> entity) {
        if (entity) {
            entities.push_back(entity);
        }
    }

    template<typename T>
    std::vector<std::shared_ptr<T>> GetEntitiesOfType() {
        std::vector<std::shared_ptr<T>> result;
        for (const auto& entity : entities) {
            if (auto typed = std::dynamic_pointer_cast<T>(entity)) {
                result.push_back(typed);
            }
        }
        return result;
    }

    std::shared_ptr<SceneEntity> FindEntityByName(const std::string& name);
};

}