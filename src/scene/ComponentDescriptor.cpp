#include "gm/scene/ComponentDescriptor.hpp"
#include "gm/scene/ComponentFactory.hpp"
#include "gm/scene/Component.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/core/Logger.hpp"
#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace gm::scene {

ComponentSchemaRegistry& ComponentSchemaRegistry::Instance() {
    static ComponentSchemaRegistry instance;
    return instance;
}

void ComponentSchemaRegistry::RegisterDescriptor(const ComponentDescriptor& descriptor) {
    if (descriptor.typeName.empty()) {
        core::Logger::Warning("[ComponentSchemaRegistry] Attempted to register descriptor with empty typeName");
        return;
    }
    
    if (m_descriptors.find(descriptor.typeName) != m_descriptors.end()) {
        core::Logger::Warning("[ComponentSchemaRegistry] Descriptor for '{}' already registered, overwriting", 
                             descriptor.typeName);
    }
    
    m_descriptors[descriptor.typeName] = descriptor;
    core::Logger::Debug("[ComponentSchemaRegistry] Registered descriptor for '{}' with {} fields", 
                       descriptor.typeName, descriptor.fields.size());
}

const ComponentDescriptor* ComponentSchemaRegistry::GetDescriptor(const std::string& typeName) const {
    auto it = m_descriptors.find(typeName);
    if (it == m_descriptors.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> ComponentSchemaRegistry::GetRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(m_descriptors.size());
    for (const auto& pair : m_descriptors) {
        types.push_back(pair.first);
    }
    return types;
}

void ComponentSchemaRegistry::Clear() {
    m_descriptors.clear();
}

std::function<nlohmann::json(Component*)> ComponentSchemaRegistry::GenerateSerializer(const ComponentDescriptor& desc) {
    if (desc.customSerialize) {
        return desc.customSerialize;
    }
    
    return [desc](Component* component) -> nlohmann::json {
        if (!component) {
            return nlohmann::json();
        }
        
        nlohmann::json data;
        data["version"] = desc.version;
        
        // For now, field-based serialization requires custom implementation
        // This is a placeholder - full implementation would use reflection or macros
        // Components can provide customSerialize for full control
        
        return data;
    };
}

std::function<Component*(GameObject*, const nlohmann::json&)> ComponentSchemaRegistry::GenerateDeserializer(const ComponentDescriptor& desc) {
    if (desc.customDeserialize) {
        return desc.customDeserialize;
    }
    
    return [desc](GameObject* obj, const nlohmann::json& data) -> Component* {
        if (!obj || !data.is_object()) {
            return nullptr;
        }
        
        auto& factory = ComponentFactory::Instance();
        auto component = factory.Create(desc.factoryName, obj);
        if (!component) {
            core::Logger::Error("[ComponentSchemaRegistry] Failed to create component '{}'", desc.typeName);
            return nullptr;
        }
        
        // For now, field-based deserialization requires custom implementation
        // This is a placeholder - full implementation would use reflection or macros
        // Components can provide customDeserialize for full control
        
        return component.get();
    };
}

} // namespace gm::scene

