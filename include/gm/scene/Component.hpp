#pragma once
#include <string>

namespace gm {

class GameObject;

class Component {
protected:
    GameObject* owner = nullptr;
    bool isActive = true;
    std::string name;

public:
    virtual ~Component() = default;

    // Override only when the component needs explicit initialization logic.
    virtual void Init() {}
    // Per-frame update hook; override for ticking behavior.
    virtual void Update(float /*deltaTime*/) {}
    // Rendering hook; override if the component draws itself.
    virtual void Render() {}
    // Called when component is about to be destroyed; override for cleanup.
    virtual void OnDestroy() {}

    void SetOwner(GameObject* obj) { owner = obj; }
    GameObject* GetOwner() const { return owner; }

    bool IsActive() const { return isActive; }
    void SetActive(bool active) { isActive = active; }

    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; }
};

}