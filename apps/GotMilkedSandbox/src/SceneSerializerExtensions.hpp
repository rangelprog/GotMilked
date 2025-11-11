#pragma once
namespace gm {
namespace SceneSerializerExtensions {

/**
 * @brief Registers sandbox-specific component serializers with the engine.
 *
 * Call RegisterSerializers() during application initialization before any
 * scenes are serialized or deserialized. Call UnregisterSerializers() on
 * shutdown if you need to remove the bindings (optional for short-lived apps).
 */

void RegisterSerializers();
void UnregisterSerializers();

} // namespace SceneSerializerExtensions
} // namespace gm

