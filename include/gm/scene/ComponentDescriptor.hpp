#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <type_traits>
#include <nlohmann/json_fwd.hpp>

namespace gm {
class Component;
class GameObject;
}

namespace gm::scene {

/**
 * @brief Field type enumeration for component descriptors
 */
enum class FieldType {
    Bool,
    Int,
    Float,
    String,
    Vec3,
    Vec4,
    Mat4,
    Enum,
    Array,
    Object,
    Custom
};

/**
 * @brief Describes a single field in a component schema
 */
struct FieldDescriptor {
    std::string name;
    FieldType type;
    std::string jsonKey;  // JSON key name (may differ from field name)
    bool required = false;
    std::string defaultValue;  // JSON string representation of default
    
    // For enum types
    std::vector<std::pair<std::string, int>> enumValues;
    
    // For custom types
    std::function<nlohmann::json(Component*)> customSerialize;
    std::function<void(Component*, const nlohmann::json&)> customDeserialize;
};

/**
 * @brief Describes a component type's serializable schema
 */
struct ComponentDescriptor {
    std::string typeName;
    std::string factoryName;  // Name used in ComponentFactory
    std::vector<FieldDescriptor> fields;
    int version = 1;
    
    // Optional custom serialization (overrides field-based)
    std::function<nlohmann::json(Component*)> customSerialize;
    std::function<Component*(GameObject*, const nlohmann::json&)> customDeserialize;
};

/**
 * @brief Registry for component descriptors
 * 
 * This allows components to self-register their schemas,
 * enabling automatic serializer generation without editing central glue code.
 */
class ComponentSchemaRegistry {
public:
    static ComponentSchemaRegistry& Instance();
    
    /**
     * @brief Register a component descriptor
     */
    void RegisterDescriptor(const ComponentDescriptor& descriptor);
    
    /**
     * @brief Get descriptor for a component type
     */
    const ComponentDescriptor* GetDescriptor(const std::string& typeName) const;
    
    /**
     * @brief Get all registered type names
     */
    std::vector<std::string> GetRegisteredTypes() const;
    
    /**
     * @brief Clear all descriptors
     */
    void Clear();
    
    /**
     * @brief Generate serializer from descriptor
     */
    static std::function<nlohmann::json(Component*)> GenerateSerializer(const ComponentDescriptor& desc);
    
    /**
     * @brief Generate deserializer from descriptor
     */
    static std::function<Component*(GameObject*, const nlohmann::json&)> GenerateDeserializer(const ComponentDescriptor& desc);

private:
    ComponentSchemaRegistry() = default;
    ~ComponentSchemaRegistry() = default;
    ComponentSchemaRegistry(const ComponentSchemaRegistry&) = delete;
    ComponentSchemaRegistry& operator=(const ComponentSchemaRegistry&) = delete;
    
    std::unordered_map<std::string, ComponentDescriptor> m_descriptors;
};

} // namespace gm::scene

