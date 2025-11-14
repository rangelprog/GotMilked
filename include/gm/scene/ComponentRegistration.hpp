#pragma once

#include "gm/scene/ComponentDescriptor.hpp"
#include "gm/scene/ComponentFactory.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/core/Logger.hpp"
#include <nlohmann/json.hpp>

/**
 * @file ComponentRegistration.hpp
 * @brief Macros and utilities for easy component registration
 * 
 * This header provides macros to register components with both the factory
 * and serializer systems, eliminating the need to edit central glue code.
 * 
 * @example
 * // In your component's source file:
 * GM_REGISTER_COMPONENT(MyComponent, "MyComponent",
 *     GM_FIELD(float, speed, "speed", false, "0.0")
 *     GM_FIELD(std::string, name, "name", false, "\"\"")
 * )
 */

namespace gm::scene {

/**
 * @brief Helper to auto-register a component with factory and serializer
 */
template<typename T>
void RegisterComponentWithSchema(const ComponentDescriptor& descriptor) {
    auto& factory = ComponentFactory::Instance();
    if (!factory.Register<T>(descriptor.factoryName)) {
        core::Logger::Warning("[ComponentRegistration] Component '{}' already registered in factory", 
                             descriptor.factoryName);
    }
    
    auto& schemaRegistry = ComponentSchemaRegistry::Instance();
    schemaRegistry.RegisterDescriptor(descriptor);
    
    // Auto-register serializer if descriptor provides custom functions
    if (descriptor.customSerialize && descriptor.customDeserialize) {
        SceneSerializer::RegisterComponentSerializer(
            descriptor.typeName,
            descriptor.customSerialize,
            descriptor.customDeserialize
        );
    }
}

} // namespace gm::scene

/**
 * @brief Register a component with factory and schema-driven serializer
 * 
 * Usage:
 * GM_REGISTER_COMPONENT(MyComponent, "MyComponent",
 *     GM_FIELD(float, speed, "speed", false, "0.0")
 *     GM_FIELD(std::string, name, "name", false, "\"\"")
 * )
 */
#define GM_REGISTER_COMPONENT(ComponentType, TypeName, ...) \
    namespace { \
        struct ComponentType##Registration { \
            ComponentType##Registration() { \
                gm::scene::ComponentDescriptor desc; \
                desc.typeName = TypeName; \
                desc.factoryName = TypeName; \
                desc.version = 1; \
                __VA_ARGS__ \
                gm::scene::RegisterComponentWithSchema<ComponentType>(desc); \
            } \
        }; \
        static ComponentType##Registration g_##ComponentType##Reg; \
    }

/**
 * @brief Define a field in a component descriptor
 */
#define GM_FIELD(Type, FieldName, JsonKey, Required, DefaultValue) \
    { \
        gm::scene::FieldDescriptor field; \
        field.name = #FieldName; \
        field.jsonKey = JsonKey; \
        field.required = Required; \
        field.defaultValue = DefaultValue; \
        if constexpr (std::is_same_v<Type, bool>) { \
            field.type = gm::scene::FieldType::Bool; \
        } else if constexpr (std::is_integral_v<Type>) { \
            field.type = gm::scene::FieldType::Int; \
        } else if constexpr (std::is_floating_point_v<Type>) { \
            field.type = gm::scene::FieldType::Float; \
        } else if constexpr (std::is_same_v<Type, std::string>) { \
            field.type = gm::scene::FieldType::String; \
        } else { \
            field.type = gm::scene::FieldType::Custom; \
        } \
        desc.fields.push_back(field); \
    }

/**
 * @brief Register a component with custom serialization functions
 * 
 * Use this when you need full control over serialization logic.
 */
#define GM_REGISTER_COMPONENT_CUSTOM(ComponentType, TypeName, SerializeFn, DeserializeFn) \
    namespace { \
        struct ComponentType##Registration { \
            ComponentType##Registration() { \
                auto& factory = gm::scene::ComponentFactory::Instance(); \
                factory.Register<ComponentType>(TypeName); \
                \
                gm::scene::ComponentDescriptor desc; \
                desc.typeName = TypeName; \
                desc.factoryName = TypeName; \
                desc.version = 1; \
                desc.customSerialize = SerializeFn; \
                desc.customDeserialize = DeserializeFn; \
                \
                auto& schemaRegistry = gm::scene::ComponentSchemaRegistry::Instance(); \
                schemaRegistry.RegisterDescriptor(desc); \
                \
                gm::SceneSerializer::RegisterComponentSerializer( \
                    TypeName, \
                    SerializeFn, \
                    DeserializeFn \
                ); \
            } \
        }; \
        static ComponentType##Registration g_##ComponentType##Reg; \
    }

