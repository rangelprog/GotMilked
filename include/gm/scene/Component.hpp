#pragma once
#include <memory>
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

    virtual void Init() {}
    virtual void Update(float deltaTime) {}
    virtual void Render() {}
    virtual void OnDestroy() {} // Called when component is about to be destroyed

    void SetOwner(GameObject* obj) { owner = obj; }
    GameObject* GetOwner() const { return owner; }

    bool IsActive() const { return isActive; }
    void SetActive(bool active) { isActive = active; }

    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; }
};

}