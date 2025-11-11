# GotMilked Engine API Reference

Complete API documentation for the GotMilked game engine.

## Table of Contents

1. [Core Systems](#core-systems)
2. [Scene Management](#scene-management)
3. [Rendering](#rendering)
4. [Physics](#physics)
5. [Input System](#input-system)
6. [Resource Management](#resource-management)
7. [Save System](#save-system)
8. [Utilities](#utilities)
9. [Gameplay](#gameplay)

---

## Core Systems

### Logger (`gm::core::Logger`)

Centralized logging system for the engine.

```cpp
namespace gm::core {
    class Logger {
    public:
        static void Info(const char* format, ...);
        static void Warning(const char* format, ...);
        static void Error(const char* format, ...);
        static void Debug(const char* format, ...);
    };
}
```

**Usage:**
```cpp
gm::core::Logger::Info("Player health: %d", health);
gm::core::Logger::Warning("Low memory: %zu bytes", bytes);
gm::core::Logger::Error("Failed to load texture: %s", path.c_str());
```

---

### Event System (`gm::core::Event`)

Event-driven architecture for decoupled communication.

```cpp
namespace gm::core {
    class Event {
    public:
        using EventCallback = std::function<void()>;
        using EventCallbackWithData = std::function<void(const void*)>;
        
        static void Subscribe(const std::string& eventName, EventCallback callback);
        static void Subscribe(const std::string& eventName, EventCallbackWithData callback);
        static void Unsubscribe(const std::string& eventName);
        static void Trigger(const std::string& eventName);
        static void Trigger(const std::string& eventName, const void* data);
    };
}
```

**Usage:**
```cpp
// Subscribe to event
gm::core::Event::Subscribe("PlayerDied", []() {
    gm::core::Logger::Info("Player died!");
});

// Trigger event
gm::core::Event::Trigger("PlayerDied");
```

---

### Input System (`gm::core::Input`)

Singleton input manager for keyboard, mouse, and gamepad input.

```cpp
namespace gm::core {
    class Input {
    public:
        static Input& Instance();
        
        bool IsActionPressed(const std::string& actionName) const;
        bool IsActionJustPressed(const std::string& actionName) const;
        bool IsActionJustReleased(const std::string& actionName) const;
        
        InputSystem* GetInputSystem();
    };
}
```

**Usage:**
```cpp
auto& input = gm::core::Input::Instance();
if (input.IsActionJustPressed("Jump")) {
    // Handle jump
}
```

---

## Scene Management

### Scene (`gm::Scene`)

Main scene container for GameObjects.

```cpp
namespace gm {
    class Scene {
    public:
        Scene(const std::string& name);
        
        std::shared_ptr<GameObject> CreateGameObject(const std::string& name);
        std::shared_ptr<GameObject> FindGameObjectByName(const std::string& name);
        std::vector<std::shared_ptr<GameObject>> GetAllGameObjects() const;
        
        void TagGameObject(std::shared_ptr<GameObject> obj, const std::string& tag);
        std::vector<std::shared_ptr<GameObject>> FindGameObjectsByTag(const std::string& tag);
        
        void Update(float deltaTime);
        void Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg);
        
        bool SaveToFile(const std::string& filepath);
        bool LoadFromFile(const std::string& filepath);
        
        void SetPaused(bool paused);
        bool IsPaused() const;
        void Cleanup();
    };
}
```

**Usage:**
```cpp
auto scene = std::make_shared<gm::Scene>("MyScene");
auto player = scene->CreateGameObject("Player");
scene->TagGameObject(player, "Player");
auto players = scene->FindGameObjectsByTag("Player");
```

---

### GameObject (`gm::GameObject`)

Entity in the scene with components.

```cpp
namespace gm {
    class GameObject {
    public:
        std::string GetName() const;
        void SetName(const std::string& name);
        
        bool IsActive() const;
        void SetActive(bool active);
        
        void AddTag(const std::string& tag);
        const std::vector<std::string>& GetTags() const;
        
        std::shared_ptr<TransformComponent> GetTransform();
        std::shared_ptr<TransformComponent> EnsureTransform();
        
        template<typename T>
        std::shared_ptr<T> AddComponent();
        
        template<typename T>
        std::shared_ptr<T> GetComponent();
        
        std::vector<std::shared_ptr<Component>> GetComponents() const;
        
        void Init();
        void Update(float deltaTime);
        void Render();
        
        bool IsDestroyed() const;
        void Destroy();
    };
}
```

**Usage:**
```cpp
auto obj = scene->CreateGameObject("MyObject");
obj->SetActive(true);
obj->AddTag("Enemy");

auto transform = obj->GetTransform();
transform->SetPosition(glm::vec3(0, 0, 0));

auto mesh = obj->AddComponent<gm::scene::StaticMeshComponent>();
```

---

### Component (`gm::Component`)

Base class for all components.

```cpp
namespace gm {
    class Component {
    public:
        virtual ~Component() = default;
        
        virtual void Init() {}
        virtual void Update(float deltaTime) {}
        virtual void Render() {}
        virtual void OnDestroy() {}
        
        std::string GetName() const;
        bool IsActive() const;
        void SetActive(bool active);
        
        GameObject* GetOwner() const;
    };
}
```

---

### TransformComponent (`gm::TransformComponent`)

Handles position, rotation, and scale.

```cpp
namespace gm {
    class TransformComponent : public Component {
    public:
        void SetPosition(const glm::vec3& position);
        glm::vec3 GetPosition() const;
        
        void SetRotation(const glm::vec3& rotation); // Euler angles in degrees
        glm::vec3 GetRotation() const;
        
        void SetScale(const glm::vec3& scale);
        glm::vec3 GetScale() const;
        
        glm::mat4 GetTransformMatrix() const;
    };
}
```

**Usage:**
```cpp
auto transform = obj->GetTransform();
transform->SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));
transform->SetRotation(glm::vec3(0.0f, 90.0f, 0.0f));
transform->SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
```

---

### StaticMeshComponent (`gm::scene::StaticMeshComponent`)

Renders a static mesh with shader and material.

```cpp
namespace gm::scene {
    class StaticMeshComponent : public Component {
    public:
        void SetMesh(Mesh* mesh, const std::string& guid = "");
        void SetShader(Shader* shader, const std::string& guid = "");
        void SetMaterial(std::shared_ptr<Material> material, const std::string& guid = "");
        
        Mesh* GetMesh() const;
        Shader* GetShader() const;
        std::shared_ptr<Material> GetMaterial() const;
        
        std::string GetMeshGuid() const;
        std::string GetShaderGuid() const;
        std::string GetMaterialGuid() const;
        
        void RestoreResources(
            std::function<Mesh*(const std::string&)> meshResolver,
            std::function<Shader*(const std::string&)> shaderResolver,
            std::function<std::shared_ptr<Material>(const std::string&)> materialResolver);
    };
}
```

**Usage:**
```cpp
auto meshComp = obj->AddComponent<gm::scene::StaticMeshComponent>();
meshComp->SetMesh(mesh, "mesh_placeholder");
meshComp->SetShader(shader, "shader_simple");
meshComp->SetMaterial(material, "material_default");
```

---

### LightComponent (`gm::LightComponent`)

Represents a light source in the scene.

```cpp
namespace gm {
    class LightComponent : public Component {
    public:
        enum class LightType {
            Directional,  // Infinite light (like sun)
            Point,        // Point light with attenuation
            Spot          // Directional light with cone angle
        };
        
        void SetType(LightType type);
        LightType GetType() const;
        
        void SetColor(const glm::vec3& color);
        const glm::vec3& GetColor() const;
        
        void SetIntensity(float intensity);
        float GetIntensity() const;
        
        void SetDirection(const glm::vec3& direction);
        const glm::vec3& GetDirection() const;
        
        void SetAttenuation(float constant, float linear, float quadratic);
        glm::vec3 GetAttenuation() const;
        
        void SetInnerConeAngle(float degrees);
        void SetOuterConeAngle(float degrees);
        float GetInnerConeAngle() const; // Returns radians
        float GetOuterConeAngle() const; // Returns radians
        
        void SetEnabled(bool enabled);
        bool IsEnabled() const;
        
        glm::vec3 GetWorldPosition() const;
        glm::vec3 GetWorldDirection() const;
    };
}
```

**Usage:**
```cpp
auto light = obj->AddComponent<gm::LightComponent>();
light->SetType(gm::LightComponent::LightType::Directional);
light->SetColor(glm::vec3(1.0f, 1.0f, 0.9f)); // Warm sunlight
light->SetIntensity(1.2f);
light->SetDirection(glm::vec3(0.3f, -1.0f, 0.2f));
```

---

### ComponentFactory (`gm::scene::ComponentFactory`)

Runtime component creation by string name.

```cpp
namespace gm::scene {
    class ComponentFactory {
    public:
        static ComponentFactory& Instance();
        
        template<typename T>
        bool Register(const std::string& typeName);
        
        bool Unregister(const std::string& typeName);
        
        std::shared_ptr<Component> Create(const std::string& typeName, GameObject* owner);
    };
}
```

**Usage:**
```cpp
auto& factory = gm::scene::ComponentFactory::Instance();
factory.Register<MyComponent>("MyComponent");

auto comp = factory.Create("MyComponent", obj.get());
```

---

### SceneSerializer (`gm::SceneSerializer`)

Serializes and deserializes scenes to/from JSON.

```cpp
namespace gm {
    class SceneSerializer {
    public:
        using SerializeCallback = std::function<nlohmann::json(Component*)>;
        using DeserializeCallback = std::function<Component*(GameObject*, const nlohmann::json&)>;
        
        static bool SaveToFile(Scene& scene, const std::string& filepath);
        static bool LoadFromFile(Scene& scene, const std::string& filepath);
        
        static std::string Serialize(Scene& scene);
        static bool Deserialize(Scene& scene, const std::string& jsonString);
        
        static void RegisterComponentSerializer(
            const std::string& typeName,
            SerializeCallback serializer,
            DeserializeCallback deserializer);
        
        static void UnregisterComponentSerializer(const std::string& typeName);
        static void ClearComponentSerializers();
    };
}
```

**Usage:**
```cpp
// Save scene
gm::SceneSerializer::SaveToFile(*scene, "my_scene.json");

// Load scene
gm::SceneSerializer::LoadFromFile(*scene, "my_scene.json");

// Register custom component serializer
gm::SceneSerializer::RegisterComponentSerializer(
    "MyComponent",
    [](Component* comp) -> nlohmann::json {
        // Serialize component
        return nlohmann::json{{"data", "value"}};
    },
    [](GameObject* obj, const nlohmann::json& json) -> Component* {
        // Deserialize component
        auto comp = obj->AddComponent<MyComponent>();
        return comp.get();
    }
);
```

---

## Rendering

### Camera (`gm::Camera`)

3D camera for view and projection matrices.

```cpp
namespace gm {
    class Camera {
    public:
        void SetPosition(const glm::vec3& position);
        glm::vec3 Position() const;
        
        void SetForward(const glm::vec3& forward);
        glm::vec3 Forward() const;
        
        glm::mat4 View() const;
        glm::mat4 Projection(float fovDegrees, float aspect, float nearPlane, float farPlane) const;
    };
}
```

**Usage:**
```cpp
gm::Camera camera;
camera.SetPosition(glm::vec3(0, 5, 10));
camera.SetForward(glm::vec3(0, 0, -1));

glm::mat4 view = camera.View();
glm::mat4 proj = camera.Projection(60.0f, 16.0f/9.0f, 0.1f, 100.0f);
```

---

### Shader (`gm::Shader`)

OpenGL shader program wrapper.

```cpp
namespace gm {
    class Shader {
    public:
        static std::shared_ptr<Shader> Create(const std::string& vertexPath, const std::string& fragmentPath);
        
        void Use();
        
        void SetBool(const std::string& name, bool value);
        void SetInt(const std::string& name, int value);
        void SetFloat(const std::string& name, float value);
        void SetVec2(const std::string& name, const glm::vec2& value);
        void SetVec3(const std::string& name, const glm::vec3& value);
        void SetVec4(const std::string& name, const glm::vec4& value);
        void SetMat4(const std::string& name, const glm::mat4& value);
    };
}
```

**Usage:**
```cpp
auto shader = gm::Shader::Create("shaders/simple.vert.glsl", "shaders/simple.frag.glsl");
shader->Use();
shader->SetVec3("uColor", glm::vec3(1.0f, 0.0f, 0.0f));
shader->SetMat4("uModel", modelMatrix);
```

---

### Mesh (`gm::Mesh`)

3D mesh data container.

```cpp
namespace gm {
    class Mesh {
    public:
        void Draw();
        void DrawInstanced(int count);
        
        // Accessors for vertex data
        // ...
    };
}
```

---

### Texture (`gm::Texture`)

Texture loading and management.

```cpp
namespace gm {
    class Texture {
    public:
        static std::shared_ptr<Texture> Load(const std::string& path);
        static std::shared_ptr<Texture> loadOrDie(const std::string& path);
        
        void Bind(unsigned int unit = 0);
        unsigned int GetID() const;
    };
}
```

**Usage:**
```cpp
auto texture = gm::Texture::Load("textures/ground.png");
texture->Bind(0);
```

---

### Material (`gm::Material`)

Material properties for rendering.

```cpp
namespace gm {
    class Material {
    public:
        void SetName(const std::string& name);
        std::string GetName() const;
        
        void SetDiffuseColor(const glm::vec3& color);
        glm::vec3 GetDiffuseColor() const;
        
        void SetSpecularColor(const glm::vec3& color);
        glm::vec3 GetSpecularColor() const;
        
        void SetShininess(float shininess);
        float GetShininess() const;
        
        void SetEmissionColor(const glm::vec3& color);
        glm::vec3 GetEmissionColor() const;
        
        void SetDiffuseTexture(std::shared_ptr<Texture> texture);
        std::shared_ptr<Texture> GetDiffuseTexture() const;
    };
}
```

**Usage:**
```cpp
auto material = std::make_shared<gm::Material>();
material->SetName("GroundMaterial");
material->SetDiffuseColor(glm::vec3(0.8f, 0.8f, 0.8f));
material->SetDiffuseTexture(texture);
```

---

## Physics

### PhysicsWorld (`gm::physics::PhysicsWorld`)

Jolt physics world singleton.

```cpp
namespace gm::physics {
    class PhysicsWorld {
    public:
        static PhysicsWorld& Instance();
        
        bool Init();
        void Step(float deltaTime);
        bool IsInitialized() const;
        
        using BodyHandle = /* implementation defined */;
        
        BodyHandle CreateBody(/* parameters */);
        void DestroyBody(BodyHandle handle);
    };
}
```

**Usage:**
```cpp
auto& physics = gm::physics::PhysicsWorld::Instance();
if (physics.Init()) {
    physics.Step(deltaTime);
}
```

---

### RigidBodyComponent (`gm::physics::RigidBodyComponent`)

Physics body component for GameObjects.

```cpp
namespace gm::physics {
    class RigidBodyComponent : public Component {
    public:
        enum class BodyType {
            Static,   // Immovable
            Dynamic   // Affected by forces
        };
        
        enum class ColliderShape {
            Plane,    // Infinite plane
            Box       // Axis-aligned box
        };
        
        void SetBodyType(BodyType type);
        BodyType GetBodyType() const;
        
        void SetColliderShape(ColliderShape shape);
        ColliderShape GetColliderShape() const;
        
        // Plane collider
        void SetPlaneNormal(const glm::vec3& normal);
        glm::vec3 GetPlaneNormal() const;
        void SetPlaneConstant(float constant);
        float GetPlaneConstant() const;
        
        // Box collider
        void SetBoxHalfExtent(const glm::vec3& halfExtent);
        glm::vec3 GetBoxHalfExtent() const;
        
        // Dynamic body
        void SetMass(float mass);
        float GetMass() const;
        
        bool IsValid() const;
        PhysicsWorld::BodyHandle GetBodyHandle() const;
    };
}
```

**Usage:**
```cpp
auto rigidBody = obj->AddComponent<gm::physics::RigidBodyComponent>();
rigidBody->SetBodyType(gm::physics::RigidBodyComponent::BodyType::Dynamic);
rigidBody->SetColliderShape(gm::physics::RigidBodyComponent::ColliderShape::Box);
rigidBody->SetBoxHalfExtent(glm::vec3(0.5f, 0.5f, 0.5f));
rigidBody->SetMass(10.0f);
```

---

## Input System

### InputBindings (`gm::core::InputBindings`)

Input action mapping system.

```cpp
namespace gm::core {
    class InputBindings {
    public:
        void BindAction(const std::string& actionName, int key);
        void UnbindAction(const std::string& actionName);
        bool IsActionBound(const std::string& actionName) const;
    };
}
```

**Usage:**
```cpp
gm::core::InputBindings bindings;
bindings.BindAction("Jump", GLFW_KEY_SPACE);
bindings.BindAction("MoveForward", GLFW_KEY_W);
```

---

## Resource Management

### ResourceManager (`gm::utils::ResourceManager`)

Centralized resource loading and caching.

```cpp
namespace gm::utils {
    class ResourceManager {
    public:
        static ResourceManager& Instance();
        
        std::shared_ptr<Shader> LoadShader(const std::string& name, 
                                          const std::string& vertPath, 
                                          const std::string& fragPath);
        
        std::shared_ptr<Texture> LoadTexture(const std::string& name, 
                                            const std::string& path);
        
        std::shared_ptr<Mesh> LoadMesh(const std::string& name, 
                                       const std::string& path);
        
        std::shared_ptr<Shader> GetShader(const std::string& name);
        std::shared_ptr<Texture> GetTexture(const std::string& name);
        std::shared_ptr<Mesh> GetMesh(const std::string& name);
    };
}
```

**Usage:**
```cpp
auto& rm = gm::utils::ResourceManager::Instance();
auto shader = rm.LoadShader("simple", "shaders/simple.vert.glsl", "shaders/simple.frag.glsl");
auto texture = rm.LoadTexture("ground", "textures/ground.png");
auto mesh = rm.LoadMesh("placeholder", "models/placeholder.obj");
```

---

### HotReloader (`gm::utils::HotReloader`)

Hot-reloads shaders, textures, and meshes during development.

```cpp
namespace gm::utils {
    class HotReloader {
    public:
        void WatchShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
        void WatchTexture(const std::string& name, const std::string& path);
        void WatchMesh(const std::string& name, const std::string& path);
        
        void Poll();
        void ForcePoll();
    };
}
```

**Usage:**
```cpp
gm::utils::HotReloader reloader;
reloader.WatchShader("simple", "shaders/simple.vert.glsl", "shaders/simple.frag.glsl");
reloader.Poll(); // Call each frame
```

---

## Save System

### SaveManager (`gm::save::SaveManager`)

Manages game save/load operations.

```cpp
namespace gm::save {
    struct SaveGameData {
        std::string version = "0.1.0";
        std::string sceneName;
        glm::vec3 cameraPosition{0.0f};
        glm::vec3 cameraForward{0.0f, 0.0f, -1.0f};
        float cameraFov = 60.0f;
        double worldTime = 0.0;
        
        int terrainResolution = 0;
        float terrainSize = 0.0f;
        float terrainMinHeight = 0.0f;
        float terrainMaxHeight = 0.0f;
        std::vector<float> terrainHeights;
    };
    
    struct SaveLoadResult {
        bool success = false;
        std::string message;
    };
    
    class SaveManager {
    public:
        explicit SaveManager(std::filesystem::path saveDirectory);
        
        SaveLoadResult QuickSave(const SaveGameData& data);
        SaveLoadResult QuickSaveWithJson(const nlohmann::json& json);
        SaveLoadResult QuickLoad(SaveGameData& outData) const;
        SaveLoadResult QuickLoadWithJson(nlohmann::json& outJson) const;
        
        SaveLoadResult SaveToSlot(const std::string& slotName, const SaveGameData& data);
        SaveLoadResult LoadFromSlot(const std::string& slotName, SaveGameData& outData) const;
        
        SaveList EnumerateSaves() const;
    };
}
```

**Usage:**
```cpp
gm::save::SaveManager saveManager("saves/");

gm::save::SaveGameData data;
data.sceneName = "MyScene";
data.cameraPosition = glm::vec3(0, 5, 10);
// ... set other data

auto result = saveManager.QuickSave(data);
if (result.success) {
    // Save successful
}

// Load
gm::save::SaveGameData loaded;
auto loadResult = saveManager.QuickLoad(loaded);
```

---

### SaveSnapshotHelpers (`gm::save::SaveSnapshotHelpers`)

Helper functions for capturing and applying game state.

```cpp
namespace gm::save {
    class SaveSnapshotHelpers {
    public:
        static SaveGameData CaptureSnapshot(
            Camera* camera,
            Scene* scene,
            std::function<double()> getWorldTime);
        
        static bool ApplySnapshot(
            const SaveGameData& data,
            Camera* camera,
            Scene* scene,
            std::function<void(double)> setWorldTime);
    };
}
```

**Usage:**
```cpp
auto data = gm::save::SaveSnapshotHelpers::CaptureSnapshot(
    camera.get(),
    scene.get(),
    [this]() { return gameplay->GetWorldTimeSeconds(); }
);

bool applied = gm::save::SaveSnapshotHelpers::ApplySnapshot(
    data,
    camera.get(),
    scene.get(),
    [this](double time) { gameplay->SetWorldTimeSeconds(time); }
);
```

---

## Utilities

### FileDialog (`gm::utils::FileDialog`)

Platform-agnostic file dialog interface.

```cpp
namespace gm::utils {
    class FileDialog {
    public:
        static std::optional<std::string> SaveFile(
            const std::string& filter,
            const std::string& defaultExtension,
            const std::string& initialDir = "",
            void* windowHandle = nullptr);
        
        static std::optional<std::string> OpenFile(
            const std::string& filter,
            const std::string& initialDir = "",
            void* windowHandle = nullptr);
    };
}
```

**Usage:**
```cpp
auto result = gm::utils::FileDialog::SaveFile(
    "JSON Files\0*.json\0All Files\0*.*\0",
    "json",
    "saves/",
    windowHandle
);

if (result.has_value()) {
    std::string filePath = result.value();
    // Save to filePath
}
```

---

### ConfigLoader (`gm::utils::ConfigLoader`)

Loads application configuration from JSON.

```cpp
namespace gm::utils {
    struct AppConfig {
        WindowConfig window;
        PathsConfig paths;
        ResourcePathConfig resources;
        std::filesystem::path configDirectory;
        HotReloadConfig hotReload;
    };
    
    class ConfigLoader {
    public:
        static ConfigLoadResult Load(const std::filesystem::path& path);
    };
}
```

**Usage:**
```cpp
auto result = gm::utils::ConfigLoader::Load("config.json");
if (result.loadedFromFile) {
    auto& config = result.config;
    // Use config.window, config.paths, etc.
}
```

---

## Gameplay

### FlyCameraController (`gm::gameplay::FlyCameraController`)

First-person camera controller.

```cpp
namespace gm::gameplay {
    class FlyCameraController {
    public:
        void SetCamera(Camera* camera);
        void SetSpeed(float speed);
        void SetSensitivity(float sensitivity);
        
        void Update(float deltaTime, Input& input);
    };
}
```

**Usage:**
```cpp
gm::gameplay::FlyCameraController controller;
controller.SetCamera(camera.get());
controller.SetSpeed(5.0f);
controller.SetSensitivity(0.1f);

// In update loop
controller.Update(deltaTime, input);
```

---

## Namespace Organization

All engine APIs are organized under the `gm` namespace:

- **Core Systems**: `gm::core::`
- **Scene Management**: `gm::` (Scene, GameObject, Component, etc.) and `gm::scene::` (StaticMeshComponent, ComponentFactory, etc.)
- **Rendering**: `gm::` (Camera, Shader, Mesh, Texture, Material)
- **Physics**: `gm::physics::`
- **Utilities**: `gm::utils::`
- **Save System**: `gm::save::`
- **Gameplay**: `gm::gameplay::`

---

## Best Practices

1. **Always use smart pointers** (`std::shared_ptr`, `std::weak_ptr`) for GameObjects and Components
2. **Use the ComponentFactory** for runtime component creation
3. **Register custom component serializers** if you create custom components
4. **Use ResourceManager** for loading assets to benefit from caching
5. **Subscribe to events** for decoupled communication
6. **Use Logger** instead of `printf` or `std::cout`
7. **Check return values** from save/load operations

---

## Examples

### Creating a Simple Scene

```cpp
#include "gm/Engine.hpp"

// Create scene
auto scene = std::make_shared<gm::Scene>("MyScene");

// Create GameObject
auto obj = scene->CreateGameObject("MyObject");

// Add Transform
auto transform = obj->GetTransform();
transform->SetPosition(glm::vec3(0, 0, 0));

// Add StaticMeshComponent
auto mesh = obj->AddComponent<gm::scene::StaticMeshComponent>();
mesh->SetMesh(myMesh, "mesh_guid");
mesh->SetShader(myShader, "shader_guid");

// Add Light
auto light = obj->AddComponent<gm::LightComponent>();
light->SetType(gm::LightComponent::LightType::Directional);
light->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
```

### Saving and Loading

```cpp
// Save scene
gm::SceneSerializer::SaveToFile(*scene, "my_scene.json");

// Load scene
gm::SceneSerializer::LoadFromFile(*scene, "my_scene.json");

// Quick save
gm::save::SaveManager saveManager("saves/");
gm::save::SaveGameData data;
// ... populate data
saveManager.QuickSave(data);
```

---

For more detailed usage examples, see:
- [Engine Usage Guide](EngineUsageGuide.md)
- [Scene Management Manual](SceneManagementManual.md)

