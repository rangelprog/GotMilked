#include "SceneSerializerExtensions.hpp"

namespace gm {
namespace SceneSerializerExtensions {

void RegisterSerializers() {
    // Register custom component serializers here when needed
    // Example:
    // gm::SceneSerializer::RegisterComponentSerializer(
    //     "MyComponent",
    //     [](gm::Component* component) -> nlohmann::json { /* serialize */ },
    //     [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* { /* deserialize */ }
    // );
}

void UnregisterSerializers() {
    // Unregister custom component serializers here when needed
    // Example:
    // gm::SceneSerializer::UnregisterComponentSerializer("MyComponent");
}

} // namespace SceneSerializerExtensions
} // namespace gm

