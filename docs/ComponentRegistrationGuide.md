# Component Registration Guide

## Overview

The schema-driven component registration system allows you to add new component types (quests, vehicles, buildings, etc.) without editing central glue code. Components can self-register using macros or explicit descriptor registration.

## Quick Start

### Option 1: Using Macros (Recommended)

Add this to your component's `.cpp` file:

```cpp
#include "gm/scene/ComponentRegistration.hpp"

// At file scope (outside any function)
GM_REGISTER_COMPONENT_CUSTOM(
    MyComponent,
    "MyComponent",
    [](gm::Component* component) -> nlohmann::json {
        auto* comp = dynamic_cast<MyComponent*>(component);
        if (!comp) return nlohmann::json();
        
        nlohmann::json data;
        data["myField"] = comp->GetMyField();
        return data;
    },
    [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
        auto& factory = gm::scene::ComponentFactory::Instance();
        auto comp = std::dynamic_pointer_cast<MyComponent>(
            factory.Create("MyComponent", obj));
        if (comp && data.contains("myField")) {
            comp->SetMyField(data["myField"].get<MyFieldType>());
        }
        return comp.get();
    }
);
```

### Option 2: Manual Registration

```cpp
#include "gm/scene/ComponentDescriptor.hpp"
#include "gm/scene/ComponentFactory.hpp"
#include "gm/scene/SceneSerializer.hpp"

namespace {
    void RegisterMyComponent() {
        // Register with factory
        auto& factory = gm::scene::ComponentFactory::Instance();
        factory.Register<MyComponent>("MyComponent");
        
        // Create descriptor
        gm::scene::ComponentDescriptor desc;
        desc.typeName = "MyComponent";
        desc.factoryName = "MyComponent";
        desc.version = 1;
        desc.customSerialize = [](gm::Component* comp) -> nlohmann::json {
            // ... serialization logic
        };
        desc.customDeserialize = [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
            // ... deserialization logic
        };
        
        // Register descriptor
        auto& registry = gm::scene::ComponentSchemaRegistry::Instance();
        registry.RegisterDescriptor(desc);
        
        // Register serializer
        gm::SceneSerializer::RegisterComponentSerializer(
            "MyComponent",
            desc.customSerialize,
            desc.customDeserialize
        );
    }
    
    // Auto-register on static initialization
    static bool g_myComponentRegistered = (RegisterMyComponent(), true);
}
```

## Migration from Old System

The old system required editing `SceneSerializerExtensions.cpp`. Now components can register themselves:

**Before:**
```cpp
// In SceneSerializerExtensions.cpp
void RegisterSerializers() {
    factory.Register<MyComponent>("MyComponent");
    SceneSerializer::RegisterComponentSerializer("MyComponent", ...);
}
```

**After:**
```cpp
// In MyComponent.cpp
GM_REGISTER_COMPONENT_CUSTOM(MyComponent, "MyComponent", ...);
```

The component will be automatically registered when its translation unit is linked.

## Benefits

1. **No Central Glue Code**: Components register themselves
2. **Type Safety**: Factory registration ensures type correctness
3. **Versioning**: Descriptors support version numbers for migration
4. **Backward Compatible**: Legacy registration still works
5. **Schema-Driven**: Future enhancement: automatic field-based serialization

## Example: Quest Component

```cpp
// In QuestTriggerComponent.cpp
#include "gm/scene/ComponentRegistration.hpp"

GM_REGISTER_COMPONENT_CUSTOM(
    gm::gameplay::QuestTriggerComponent,
    "QuestTriggerComponent",
    [](gm::Component* component) -> nlohmann::json {
        auto* quest = dynamic_cast<gm::gameplay::QuestTriggerComponent*>(component);
        if (!quest) return nlohmann::json();
        
        nlohmann::json data;
        data["questId"] = quest->GetQuestId();
        data["activationRadius"] = quest->GetActivationRadius();
        data["triggerOnSceneLoad"] = quest->TriggerOnSceneLoad();
        data["triggerOnInteract"] = quest->TriggerOnInteract();
        data["repeatable"] = quest->IsRepeatable();
        data["activationAction"] = quest->GetActivationAction();
        return data;
    },
    [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
        if (!data.is_object()) return nullptr;
        
        auto& factory = gm::scene::ComponentFactory::Instance();
        auto quest = std::dynamic_pointer_cast<gm::gameplay::QuestTriggerComponent>(
            factory.Create("QuestTriggerComponent", obj));
        if (!quest) return nullptr;
        
        if (data.contains("questId")) quest->SetQuestId(data["questId"].get<std::string>());
        if (data.contains("activationRadius")) quest->SetActivationRadius(data["activationRadius"].get<float>());
        if (data.contains("triggerOnSceneLoad")) quest->SetTriggerOnSceneLoad(data["triggerOnSceneLoad"].get<bool>());
        if (data.contains("triggerOnInteract")) quest->SetTriggerOnInteract(data["triggerOnInteract"].get<bool>());
        if (data.contains("repeatable")) quest->SetRepeatable(data["repeatable"].get<bool>());
        if (data.contains("activationAction")) quest->SetActivationAction(data["activationAction"].get<std::string>());
        
        return quest.get();
    }
);
```

## Future: Field-Based Serialization

The descriptor system is designed to support automatic field-based serialization in the future. When implemented, you'll be able to write:

```cpp
GM_REGISTER_COMPONENT(MyComponent, "MyComponent",
    GM_FIELD(float, speed, "speed", false, "0.0")
    GM_FIELD(std::string, name, "name", false, "\"\"")
    GM_FIELD(bool, enabled, "enabled", false, "true")
)
```

And serialization will be generated automatically from the field descriptors.

