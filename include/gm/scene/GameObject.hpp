#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <typeindex>
#include "Component.hpp"

namespace gm {

class TransformComponent;

class GameObject {
private:
    std::vector<std::shared_ptr<Component>> components;
    mutable std::unordered_map<std::type_index, std::shared_ptr<Component>> componentCache;
    bool isActive = true;
    bool isDestroyed = false;
    std::string name;
    std::unordered_set<std::string> tags;
    int layer = 0;  // For layer-based queries/rendering

    void InvalidateCache() const;

public:
    GameObject() = default;
    explicit GameObject(const std::string& objectName) : name(objectName) {}
    virtual ~GameObject() = default;

    // Lifecycle
    virtual void Init();
    virtual void Update(float deltaTime);
    virtual void Render();

    // Component management
    template<typename T>
    std::shared_ptr<T> AddComponent() {
        auto component = std::make_shared<T>();
        component->SetOwner(this);
        components.push_back(component);
        InvalidateCache(); // Cache is invalidated when components are added
        return component;
    }

    template<typename T>
    std::shared_ptr<T> GetComponent() const {
        std::type_index typeId = std::type_index(typeid(T));
        
        // Check cache first
        auto cacheIt = componentCache.find(typeId);
        if (cacheIt != componentCache.end()) {
            return std::dynamic_pointer_cast<T>(cacheIt->second);
        }
        
        // Search components
        for (auto& component : components) {
            if (auto result = std::dynamic_pointer_cast<T>(component)) {
                // Cache the result
                componentCache[typeId] = result;
                return result;
            }
        }
        
        // Cache nullptr to avoid repeated searches
        componentCache[typeId] = nullptr;
        return nullptr;
    }

    template<typename T>
    std::vector<std::shared_ptr<T>> GetComponents() const {
        std::vector<std::shared_ptr<T>> results;
        for (auto& component : components) {
            if (auto result = std::dynamic_pointer_cast<T>(component)) {
                results.push_back(result);
            }
        }
        return results;
    }

    template<typename T>
    bool HasComponent() const {
        return GetComponent<T>() != nullptr;
    }

    template<typename T>
    bool RemoveComponent() {
        std::type_index typeId = std::type_index(typeid(T));
        std::vector<std::shared_ptr<Component>> toDestroy;
        
        // Find and collect components to destroy
        auto it = std::remove_if(components.begin(), components.end(),
            [&toDestroy](const std::shared_ptr<Component>& component) {
                if (std::dynamic_pointer_cast<T>(component) != nullptr) {
                    toDestroy.push_back(component);
                    return true;
                }
                return false;
            });
        
        bool removed = (it != components.end());
        if (removed) {
            // Call OnDestroy before removing
            for (auto& component : toDestroy) {
                if (component) {
                    component->OnDestroy();
                }
            }
            components.erase(it, components.end());
            InvalidateCache();
        }
        return removed;
    }

    bool RemoveComponent(std::shared_ptr<Component> component) {
        auto it = std::find(components.begin(), components.end(), component);
        if (it != components.end()) {
            (*it)->OnDestroy(); // Call OnDestroy before removing
            components.erase(it);
            InvalidateCache();
            return true;
        }
        return false;
    }

    std::vector<std::shared_ptr<Component>>& GetComponents() { return components; }
    const std::vector<std::shared_ptr<Component>>& GetComponents() const { return components; }

    // State management
    bool IsActive() const { return isActive; }
    void SetActive(bool active) { isActive = active; }

    bool IsDestroyed() const { return isDestroyed; }
    virtual void Destroy(); // Calls OnDestroy on all components, then marks as destroyed

    // Naming
    const std::string& GetName() const { return name; }
    void SetName(const std::string& newName) { name = newName; }

    // Tags
    void AddTag(const std::string& tag) { tags.insert(tag); }
    void RemoveTag(const std::string& tag) { tags.erase(tag); }
    bool HasTag(const std::string& tag) const { return tags.find(tag) != tags.end(); }
    const std::unordered_set<std::string>& GetTags() const { return tags; }

    // Layers
    int GetLayer() const { return layer; }
    void SetLayer(int newLayer) { layer = newLayer; }

    // Transform helper methods
    std::shared_ptr<TransformComponent> GetTransform() const;
    std::shared_ptr<TransformComponent> EnsureTransform(); // Gets existing or creates new
};

}