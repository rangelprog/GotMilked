#pragma once
#include <vector>
#include <memory>
#include <string>
#include "Component.hpp"

namespace gm {

class GameObject {
private:
    std::vector<std::shared_ptr<Component>> components;
    bool isActive = true;
    std::string name;

public:
    GameObject() = default;
    virtual ~GameObject() = default;

    virtual void Init();
    virtual void Update(float deltaTime);
    virtual void Render();

    template<typename T>
    std::shared_ptr<T> AddComponent() {
        auto component = std::make_shared<T>();
        component->SetOwner(this);
        components.push_back(component);
        return component;
    }

    template<typename T>
    std::shared_ptr<T> GetComponent() {
        for (auto& component : components) {
            if (auto result = std::dynamic_pointer_cast<T>(component)) {
                return result;
            }
        }
        return nullptr;
    }

    bool IsActive() const { return isActive; }
    void SetActive(bool active) { isActive = active; }

    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; }
};

}