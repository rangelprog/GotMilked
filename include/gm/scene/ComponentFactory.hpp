#pragma once

#include "gm/scene/Component.hpp"
#include "gm/scene/GameObject.hpp"

#include <string>
#include <functional>
#include <unordered_map>
#include <memory>
#include <type_traits>
#include <vector>

namespace gm::scene {

/**
 * @brief Factory for creating components by name string.
 * 
 * This factory pattern allows runtime component creation from string names,
 * which is essential for serialization/deserialization and dynamic component
 * creation. Components must be registered before they can be created.
 * 
 * @example
 * // Register a component type (typically done during initialization)
 * auto& factory = ComponentFactory::Instance();
 * factory.Register<MyComponent>("MyComponent");
 * 
 * // Create a component by name at runtime
 * auto component = factory.Create("MyComponent", gameObject);
 * if (auto myComp = std::dynamic_pointer_cast<MyComponent>(component)) {
 *     // Use the component
 * }
 */
class ComponentFactory {
public:
    using CreatorFunc = std::function<std::shared_ptr<Component>(GameObject*)>;

    /**
     * @brief Get the singleton instance of the factory.
     */
    static ComponentFactory& Instance();

    /**
     * @brief Register a component type with the factory.
     * 
     * @tparam T Component type (must inherit from Component)
     * @param typeName String identifier for the component type
     * @return true if registration succeeded, false if typeName already exists
     */
    template<typename T>
    bool Register(const std::string& typeName) {
        static_assert(std::is_base_of_v<Component, T>, 
                     "T must inherit from Component");
        
        if (m_creators.find(typeName) != m_creators.end()) {
            return false; // Already registered
        }
        
        m_creators[typeName] = [](GameObject* obj) -> std::shared_ptr<Component> {
            if (!obj) {
                return nullptr;
            }
            return obj->AddComponent<T>();
        };
        
        return true;
    }

    /**
     * @brief Unregister a component type.
     * 
     * @param typeName String identifier for the component type
     * @return true if unregistered, false if typeName was not found
     */
    bool Unregister(const std::string& typeName);

    /**
     * @brief Create a component by name.
     * 
     * @param typeName String identifier for the component type
     * @param obj GameObject to attach the component to
     * @return Shared pointer to the created component, or nullptr if creation failed
     */
    std::shared_ptr<Component> Create(const std::string& typeName, GameObject* obj) const;

    /**
     * @brief Check if a component type is registered.
     * 
     * @param typeName String identifier for the component type
     * @return true if registered, false otherwise
     */
    bool IsRegistered(const std::string& typeName) const;

    /**
     * @brief Get all registered component type names.
     * 
     * @return Vector of registered type names
     */
    std::vector<std::string> GetRegisteredTypes() const;

    /**
     * @brief Clear all registered component types.
     */
    void Clear();

private:
    ComponentFactory() = default;
    ~ComponentFactory() = default;
    ComponentFactory(const ComponentFactory&) = delete;
    ComponentFactory& operator=(const ComponentFactory&) = delete;

    std::unordered_map<std::string, CreatorFunc> m_creators;
};

} // namespace gm::scene

