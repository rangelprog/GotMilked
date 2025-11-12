#pragma once
#include <string>
#include <typeindex>

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
    // Called when component is being reused from a pool; override to reset state.
    virtual void OnReset() { isActive = true; }

    void SetOwner(GameObject* obj) { owner = obj; }
    GameObject* GetOwner() const { return owner; }

    bool IsActive() const { return isActive; }
    void SetActive(bool active) { isActive = active; }

    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; }

    static const std::string& TypeName(const std::type_index& typeId);

    template<typename T>
    static const std::string& TypeName() {
        return TypeName(std::type_index(typeid(T)));
    }

    static const std::string& TypeName(const Component& component) {
        return TypeName(std::type_index(typeid(component)));
    }
};

}