# Scene Management Manual

This manual explains how to create, update, serialize, and manage scenes in the GotMilked engine. It is meant as a practical guide for everyday workflows.

---

## 1. Core Concepts

- **Scene**: The top-level container for game objects, components, and systems. Defined in `gm::Scene`.
- **GameObject**: A named entity inside a scene. Owns a set of components and metadata.
- **Component**: Behaviour or data attached to a `GameObject` (e.g., transform, material, light).
- **SceneManager**: Singleton that owns all scenes, handles loading/unloading, and tracks the active scene.

---

## 2. Typical Scene Lifecycle

### 2.1 Creating a Scene

```cpp
auto& sceneManager = gm::SceneManager::Instance();
auto scene = sceneManager.CreateScene("Level1");
sceneManager.SetActiveScene("Level1");
```

- `CreateScene` registers a new scene by name.
- `SetActiveScene` marks it as current for update calls.

### 2.2 Initializing and Updating

```cpp
scene->Init();              // Call once before the first update
scene->Update(deltaTime);   // Call every frame
scene->Draw(shader, camera, width, height, fovDegrees);
```

- `Init()` propagates to all active game objects.
- `Update()` skips execution when the scene is paused.
- `Draw()` handles lighting and rendering for every active `GameObject`.

### 2.3 Cleaning Up

- `scene->Cleanup();` removes all game objects and resets internal state.
- Called automatically when `SceneSerializer::Deserialize` loads over an existing scene.
- When unloading via `SceneManager::UnloadScene`, cleanup occurs internally.

---

## 3. Working with Game Objects

### 3.1 Creating Objects

```cpp
auto player = scene->CreateGameObject("Player");
auto spawnedEnemy = scene->SpawnGameObject("Enemy");
```

- `CreateGameObject` registers the object but does not call `Init` (useful during setup).
- `SpawnGameObject` will immediately initialize the object if the scene was already initialized.

### 3.2 Destroying Objects

```cpp
scene->DestroyGameObject(player);
scene->DestroyGameObjectByName("Enemy_01");
```

- Destruction is deferred until `Scene::CleanupDestroyedObjects` runs at the end of the frame.
- This avoids iterator invalidation during updates.

### 3.3 Tags and Layers

```cpp
scene->TagGameObject(player, "player");
scene->UntagGameObject(player, "player");

auto enemies = scene->FindGameObjectsByTag("enemy");
auto boss = scene->FindGameObjectByName("Boss");
```

- Tags support fast group queries (`O(k)` where `k` is count of tagged objects).
- Layers are integer markers stored per game object (manual filtering when needed).

### 3.4 Accessing Components

```cpp
auto transform = player->EnsureTransform();           // Always returns a TransformComponent
auto material = player->AddComponent<MaterialComponent>();
auto light = player->GetComponent<LightComponent>();
```

- `EnsureTransform` returns the existing transform or creates one if absent.
- Components are cached by type, so repeated `GetComponent` lookups are fast.

### 3.5 Component Lifecycle Hooks

Components inherit the `gm::Component` base class, which exposes four virtual hooks:

| Hook          | When it fires                                        | Typical use                                  |
|---------------|------------------------------------------------------|----------------------------------------------|
| `Init()`      | When the owning `GameObject` initializes             | Acquire resources, cache lookups             |
| `Update(dt)`  | Once per frame while the component is active         | Behaviour, ticking logic                     |
| `Render()`    | During the owning scene’s `Draw` call                | Submit draw calls (rare outside render comps)|
| `OnDestroy()` | Before the `GameObject` is removed or component freed| Release handles, unregister from services    |

Overrides are optional—only implement the hooks you need. `GameObject::Init/Update/Render/Destroy` automatically fan out to all active components, so you rarely invoke these hooks directly.

---

## 4. Scene Serialization (Save/Load)

### 4.1 Saving to Disk

```cpp
scene->SaveToFile("assets/scenes/tutorial.json");
// Under the hood: SceneSerializer::SaveToFile
```

- Serializes scene metadata, active state, tags, and supported components.
- Only active, non-destroyed game objects are written.

### 4.2 Loading from Disk

```cpp
scene->LoadFromFile("assets/scenes/tutorial.json");
// Under the hood: SceneSerializer::LoadFromFile
```

- Clears the scene before loading new data.
- Reconstructs game objects, tags, transform/material/light components, and sets component active flags.

### 4.3 JSON Layout (Simplified)

```json
{
  "name": "Tutorial",
  "isPaused": false,
  "gameObjects": [
    {
      "name": "Player",
      "active": true,
      "tags": ["player", "ally"],
      "components": [
        {
          "type": "TransformComponent",
          "active": true,
          "data": {
            "position": [0.0, 1.0, 0.0],
            "rotation": [0.0, 0.0, 0.0],
            "scale": [1.0, 1.0, 1.0]
          }
        }
      ]
    }
  ]
}
```

### 4.4 Supported Components

| Component             | Serialized Fields                                           |
|-----------------------|-------------------------------------------------------------|
| TransformComponent    | `position`, `rotation`, `scale`                             |
| MaterialComponent     | `name`, `diffuseColor`, `specularColor`, `shininess`, `emissionColor` |
| LightComponent        | `type`, `color`, `intensity`, `enabled`, `direction`, `attenuation`, `innerConeAngle`, `outerConeAngle` |

> **Note:** Light cone angles are stored and serialized in degrees; they are converted to radians internally.

### 4.5 Adding Custom Components

When you add a new component type:

1. Extend `SerializeComponent` and `DeserializeComponent` in `SceneSerializer.cpp`.
2. Provide `SerializeYourComponent` / `DeserializeYourComponent` helpers.
3. Update the JSON schema documentation above.

### 4.6 Registering Custom Serializers

The engine exposes a plug-in API so games can register serializers without touching core code:

```cpp
gm::SceneSerializer::RegisterComponentSerializer(
    "MeshSpinnerComponent",
    [](gm::Component* component) -> nlohmann::json {
        auto* spinner = dynamic_cast<MeshSpinnerComponent*>(component);
        nlohmann::json data;
        data["rotationSpeed"] = spinner->GetRotationSpeed();
        data["meshPath"] = spinner->GetMeshPath();
        return data;
    },
    [](gm::GameObject* obj, const nlohmann::json& data) -> gm::Component* {
        auto component = obj->AddComponent<MeshSpinnerComponent>();
        if (data.contains("rotationSpeed"))
            component->SetRotationSpeed(data["rotationSpeed"].get<float>());
        component->SetMeshPath(data.value("meshPath", ""));
        return component.get();
    });
```

Call `RegisterComponentSerializer` during startup (before saving or loading scenes) and optionally `UnregisterComponentSerializer` on shutdown. Games can provide thin wrappers (see `SceneSerializerExtensions` in the sandbox) to keep registration code organized.

---

## 5. Managing Multiple Scenes with SceneManager

### 5.1 Loading and Activating

```cpp
auto& sceneManager = gm::SceneManager::Instance();
sceneManager.LoadScene("MainMenu");          // Returns a shared_ptr<Scene>
sceneManager.SetActiveScene("MainMenu");
```

### 5.2 Switching Scenes

```cpp
sceneManager.UnloadScene("MainMenu");
sceneManager.LoadScene("Level1");
sceneManager.SetActiveScene("Level1");
```

- `UnloadScene` triggers cleanup and removes it from the registry.
- Loading an already loaded scene returns the existing instance.

### 5.3 Updating the Active Scene

```cpp
sceneManager.UpdateActiveScene(deltaTime);
```

- Delegates `Update` to the currently active scene.
- Safe to call even if no active scene is set (internally guarded).

---

## 6. Patterns & Recipes

### 6.1 Enemy Spawner Loop

```cpp
class EnemySpawner {
    std::string sceneName{"Level1"};
    float spawnTimer = 0.0f;
    float spawnInterval = 2.0f;
    int maxEnemies = 10;

public:
    void Update(float deltaTime) {
        auto scene = gm::SceneManager::Instance().GetScene(sceneName);
        if (!scene) return;

        auto enemies = scene->FindGameObjectsByTag("enemy");
        if (enemies.size() >= maxEnemies) return;

        spawnTimer += deltaTime;
        if (spawnTimer >= spawnInterval) {
            SpawnEnemy(scene);
            spawnTimer = 0.0f;
        }

        for (auto& enemy : enemies) {
            auto health = enemy->GetComponent<HealthComponent>();
            if (health && health->IsDead()) {
                scene->DestroyGameObject(enemy);
            }
        }
    }

private:
    void SpawnEnemy(const std::shared_ptr<gm::Scene>& scene) {
        static int enemyCount = 0;
        auto enemy = scene->SpawnGameObject("Enemy_" + std::to_string(enemyCount++));
        enemy->SetLayer(1);
        scene->TagGameObject(enemy, "enemy");
        enemy->AddComponent<HealthComponent>()->SetMaxHealth(100);
        enemy->AddComponent<AIComponent>()->SetBehavior(AIBehavior::Patrol);
    }
};
```

### 6.2 Multi-Scene Flow

```cpp
auto& sceneManager = gm::SceneManager::Instance();
auto menuScene = sceneManager.LoadScene("Menu");
sceneManager.SetActiveScene("Menu");
// ... menu logic ...

sceneManager.UnloadScene("Menu");
auto levelScene = sceneManager.LoadScene("Level1");
sceneManager.SetActiveScene("Level1");
sceneManager.InitActiveScene();
```

### 6.3 Tag-Based Gameplay Actions

```cpp
auto scene = gm::SceneManager::Instance().GetActiveScene();
auto interactables = scene->FindGameObjectsByTag("interactable");
for (auto& obj : interactables) {
    auto component = obj->GetComponent<InteractableComponent>();
    if (component && PlayerNear(obj)) {
        component->OnPlayerNear();
    }
}

auto projectiles = scene->FindGameObjectsByTag("projectile");
for (auto& projectile : projectiles) {
    if (projectile->IsDestroyed()) continue;
    projectile->GetComponent<ProjectileComponent>()->UpdateFlight();
}
```

### 6.4 Game Loop Integration

```cpp
while (!glfwWindowShouldClose(window)) {
    input->Poll();
    auto& sceneManager = gm::SceneManager::Instance();
    sceneManager.UpdateActiveScene(deltaTime);

    auto scene = sceneManager.GetActiveScene();
    if (scene) {
        scene->Draw(shader, camera, framebufferWidth, framebufferHeight, fovDegrees);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
}

sceneManager.Shutdown();
```

---

## 7. Best Practices

1. **Unique Names**  
   - `Scene::CreateGameObject` logs a warning and returns the existing object if the name already exists. Use unique names for consistent serialization.

2. **Tag Through Scene**  
   - Always call `Scene::TagGameObject` so that internal tag maps stay in sync. Do not rely solely on `GameObject::AddTag`.

3. **Deferred Destruction**  
   - Because deletion is deferred, do not hold raw pointers to game objects that may be destroyed mid-frame. Use `std::shared_ptr` handles from the scene.

4. **Serialization Safety**  
   - Ensure materials and lights have valid data before saving. Missing textures or malformed vectors can break runtime expectations.

5. **Pause State**  
   - Use `scene->SetPaused(true)` to temporarily halt updates (e.g., pause menu). Rendering still occurs unless handled separately.

6. **Version Control**  
   - Keep serialized scene files (`.json`) under source control. They represent level content and should receive the same review as code changes.

---

## 8. Troubleshooting

| Issue | Likely Cause | Fix |
|-------|--------------|-----|
| Game object missing after load | Duplicate name during creation | Check logs for `[Scene] Warning: GameObject with name ... already exists` |
| Tags not restored | Tag added directly on `GameObject` instead of via `Scene::TagGameObject` | Always tag through the scene or rely on serializer |
| Component inactive after load | Component serialized with `"active": false` | Set to `true` in JSON or enable manually after loading |
| Lights behave differently after load | Cone angles stored in JSON are too small/large | JSON values are degrees; verify they match expected spotlight cone in degrees |
| Scene merge required instead of replace | `SceneSerializer` currently clears the scene before loading | Implement a custom merge routine (future extension) |

---

## 9. Extending the System

- **Custom Serialization**: Add support for new components by extending `SceneSerializer`.
- **Editor Integration**: Hook `SaveToFile`/`LoadFromFile` into tooling UI.
- **Runtime Scene Streaming**: Extend `SceneManager` to load scenes asynchronously or in chunks.
- **Physics & AI**: Build subsystems on top of the tagging system for queries and spatial partitioning.

---

## 10. Useful File References

- `include/gm/scene/Scene.hpp` – Scene interface.
- `src/scene/Scene.cpp` – Scene implementation.
- `include/gm/scene/GameObject.hpp` – GameObject and component management.
- `include/gm/scene/SceneManager.hpp` / `src/scene/SceneManager.cpp` – SceneManager API.
- `include/gm/scene/SceneSerializer.hpp` / `src/scene/SceneSerializer.cpp` – Serialization logic.

---

## 11. Quick Start Checklist

- [ ] Create or load a scene via `SceneManager`.
- [ ] Use `SpawnGameObject` to add new entities.
- [ ] Attach and configure components (transform/material/light).
- [ ] Tag objects for gameplay queries.
- [ ] Call `Update` and `Draw` each frame.
- [ ] Save with `Scene::SaveToFile` before exiting.
- [ ] Load with `Scene::LoadFromFile` on startup.

---

With these steps, you can confidently create levels, manage game objects, and persist scenes across editor and runtime sessions. Happy world building!

