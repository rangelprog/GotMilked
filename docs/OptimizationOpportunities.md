# Optimization Opportunities

This document identifies performance bottlenecks and optimization opportunities in the GotMilked engine.

## Table of Contents

1. [Critical Performance Issues](#critical-performance-issues)
2. [Memory Optimizations](#memory-optimizations)
3. [Rendering Optimizations](#rendering-optimizations)
4. [Scene Management Optimizations](#scene-management-optimizations)
5. [Physics Optimizations](#physics-optimizations)
6. [Resource Management Optimizations](#resource-management-optimizations)
7. [String and Allocation Optimizations](#string-and-allocation-optimizations)
8. [Profiling Recommendations](#profiling-recommendations)

---

## Critical Performance Issues

### 1. LightManager Created Every Frame

**Location:** `src/scene/Scene.cpp:425`

**Problem:**
```cpp
void Scene::Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
    // ...
    LightManager lightManager;  // ❌ Created every frame!
    lightManager.CollectLights(gameObjects);
    lightManager.ApplyLights(shader, cam.Position());
}
```

**Impact:** High - Creates a new `LightManager` object every frame, causing unnecessary allocations.

**Solution:**
```cpp
class Scene {
private:
    LightManager m_lightManager;  // Cache the LightManager
    
public:
    void Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
        // ...
        m_lightManager.CollectLights(gameObjects);
        m_lightManager.ApplyLights(shader, cam.Position());
    }
};
```

**Expected Improvement:** ~5-10% frame time reduction

---

### 2. Linear GameObject Traversal Every Frame

**Location:** `src/scene/Scene.cpp:430-434`

**Problem:**
```cpp
// Draw GameObjects
for (const auto& gameObject : gameObjects) {
    if (gameObject && gameObject->IsActive() && !gameObject->IsDestroyed()) {
        gameObject->Render();
    }
}
```

**Impact:** Medium-High - Linear search through all GameObjects every frame, even inactive ones.

**Solution:** Maintain separate render lists for active GameObjects:
```cpp
class Scene {
private:
    std::vector<std::shared_ptr<GameObject>> m_activeRenderables;
    std::vector<std::shared_ptr<GameObject>> m_activeUpdatables;
    
    void UpdateRenderLists() {
        m_activeRenderables.clear();
        m_activeUpdatables.clear();
        
        for (auto& obj : gameObjects) {
            if (obj && !obj->IsDestroyed()) {
                if (obj->IsActive()) {
                    m_activeRenderables.push_back(obj);
                    m_activeUpdatables.push_back(obj);
                }
            }
        }
    }
    
public:
    void Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
        // Only iterate over active renderables
        for (const auto& gameObject : m_activeRenderables) {
            gameObject->Render();
        }
    }
};
```

**Expected Improvement:** ~10-20% frame time reduction with many inactive objects

---

### 3. Component Lookup Using Linear Search

**Location:** `src/scene/GameObject.cpp` (GetComponent template)

**Problem:**
```cpp
template<typename T>
std::shared_ptr<T> GameObject::GetComponent() const {
    // Linear search through all components
    for (auto& component : components) {
        if (auto cast = std::dynamic_pointer_cast<T>(component)) {
            return cast;
        }
    }
    return nullptr;
}
```

**Impact:** Medium - `dynamic_cast` is expensive, and linear search is O(n).

**Solution:** Use type_index-based component storage:
```cpp
class GameObject {
private:
    std::unordered_map<std::type_index, std::shared_ptr<Component>> m_componentMap;
    std::vector<std::shared_ptr<Component>> m_components;  // Keep for iteration
    
public:
    template<typename T>
    std::shared_ptr<T> GetComponent() const {
        auto it = m_componentMap.find(std::type_index(typeid(T)));
        if (it != m_componentMap.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        return nullptr;
    }
    
    template<typename T>
    std::shared_ptr<T> AddComponent() {
        auto comp = std::make_shared<T>();
        comp->owner = this;
        m_components.push_back(comp);
        m_componentMap[std::type_index(typeid(T))] = comp;
        InvalidateCache();
        return comp;
    }
};
```

**Expected Improvement:** ~5-15% improvement in component-heavy scenes

---

## Memory Optimizations

### 4. String Copies in Hot Paths

**Location:** Throughout codebase, especially in serialization and logging

**Problem:**
```cpp
// Many string copies
std::string name = obj->GetName();  // Copy
objJson["name"] = name;  // Another copy
```

**Solution:** Use string_view where possible, reserve string capacity:
```cpp
// Use string_view for read-only access
std::string_view GetNameView() const { return m_name; }

// Reserve capacity for known sizes
std::string jsonString;
jsonString.reserve(estimatedSize);
```

**Expected Improvement:** Reduced allocations, ~2-5% improvement

---

### 5. Vector Reallocations

**Location:** `src/scene/Scene.cpp`, component vectors

**Problem:**
```cpp
gameObjects.push_back(gameObject);  // May cause reallocation
```

**Solution:** Reserve capacity when known:
```cpp
// In Scene constructor or Init
gameObjects.reserve(estimatedObjectCount);

// For components
components.reserve(estimatedComponentCount);
```

**Expected Improvement:** Reduced frame spikes during growth

---

### 6. JSON Serialization Memory

**Location:** `src/scene/SceneSerializer.cpp`

**Problem:**
```cpp
std::string SceneSerializer::Serialize(Scene& scene) {
    json sceneJson;
    // ... build JSON
    return sceneJson.dump(4);  // Creates large string
}
```

**Solution:** Use streaming or binary format for large scenes:
```cpp
// Stream directly to file instead of building in memory
void SceneSerializer::SerializeToStream(Scene& scene, std::ostream& stream) {
    // Write incrementally
}
```

**Expected Improvement:** Reduced memory usage for large scenes

---

## Rendering Optimizations

### 7. Shader Uniform Updates Every Frame

**Location:** `src/scene/Scene.cpp:419-422`

**Problem:**
```cpp
shader.Use();
shader.SetMat4("uView", view);
shader.SetMat4("uProj", proj);
shader.SetVec3("uViewPos", cam.Position());
```

**Impact:** Low-Medium - If view/proj don't change, this is wasted.

**Solution:** Cache and compare:
```cpp
class Scene {
private:
    glm::mat4 m_lastView;
    glm::mat4 m_lastProj;
    glm::vec3 m_lastViewPos;
    
public:
    void Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
        glm::mat4 view = cam.View();
        glm::mat4 proj = /* ... */;
        glm::vec3 viewPos = cam.Position();
        
        shader.Use();
        
        if (view != m_lastView) {
            shader.SetMat4("uView", view);
            m_lastView = view;
        }
        
        if (proj != m_lastProj) {
            shader.SetMat4("uProj", proj);
            m_lastProj = proj;
        }
        
        if (viewPos != m_lastViewPos) {
            shader.SetVec3("uViewPos", viewPos);
            m_lastViewPos = viewPos;
        }
    }
};
```

**Expected Improvement:** ~1-3% improvement if camera is static

---

### 8. Frustum Culling

**Location:** `src/scene/Scene.cpp:430-434`

**Problem:** All GameObjects are rendered, even if outside view frustum.

**Solution:** Implement frustum culling:
```cpp
class Scene {
private:
    struct Frustum {
        glm::vec4 planes[6];
    };
    
    Frustum CalculateFrustum(const glm::mat4& viewProj);
    bool IsInFrustum(const GameObject& obj, const Frustum& frustum);
    
public:
    void Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
        glm::mat4 viewProj = proj * view;
        Frustum frustum = CalculateFrustum(viewProj);
        
        for (const auto& gameObject : m_activeRenderables) {
            if (IsInFrustum(*gameObject, frustum)) {
                gameObject->Render();
            }
        }
    }
};
```

**Expected Improvement:** 20-50% improvement in scenes with many off-screen objects

---

### 9. Instanced Rendering

**Location:** StaticMeshComponent rendering

**Problem:** Each GameObject with the same mesh renders separately.

**Solution:** Batch render identical meshes:
```cpp
class Scene {
private:
    struct RenderBatch {
        Mesh* mesh;
        Shader* shader;
        Material* material;
        std::vector<glm::mat4> transforms;
    };
    
    void BatchRenderables() {
        // Group by mesh/shader/material
        // Collect transforms
        // Render with instancing
    }
};
```

**Expected Improvement:** 30-70% improvement for scenes with many identical objects

---

## Scene Management Optimizations

### 10. GameObject Name Lookup

**Location:** `src/scene/Scene.cpp:279`

**Problem:**
```cpp
std::shared_ptr<GameObject> Scene::FindGameObjectByName(const std::string& name) {
    for (const auto& obj : gameObjects) {
        if (obj && obj->GetName() == name) {
            return obj;
        }
    }
    return nullptr;
}
```

**Impact:** Medium - Linear search O(n).

**Solution:** Already has `objectsByName` map, but not always used:
```cpp
std::shared_ptr<GameObject> Scene::FindGameObjectByName(const std::string& name) {
    auto it = objectsByName.find(name);
    if (it != objectsByName.end() && !it->second->IsDestroyed()) {
        return it->second;
    }
    return nullptr;
}
```

**Expected Improvement:** O(1) lookup instead of O(n)

---

### 11. Tag Lookup Optimization

**Location:** `src/scene/Scene.cpp:360`

**Problem:** Similar to name lookup, but tags are stored in a map.

**Solution:** Ensure `objectsByTag` is properly maintained and used.

**Expected Improvement:** Already optimized if map is used correctly

---

### 12. Destroyed Object Cleanup

**Location:** `src/scene/Scene.cpp:CleanupDestroyedObjects()`

**Problem:** May be called every frame, causing allocations.

**Solution:** Batch cleanup or use object pooling:
```cpp
void Scene::CleanupDestroyedObjects() {
    // Only cleanup every N frames or when count exceeds threshold
    static int frameCounter = 0;
    if (++frameCounter < 60) return;  // Every 60 frames
    frameCounter = 0;
    
    // Use erase-remove idiom
    gameObjects.erase(
        std::remove_if(gameObjects.begin(), gameObjects.end(),
            [](const auto& obj) { return !obj || obj->IsDestroyed(); }),
        gameObjects.end()
    );
}
```

**Expected Improvement:** Reduced frame spikes

---

## Physics Optimizations

### 13. Physics Body Creation

**Location:** `src/physics/RigidBodyComponent.cpp`

**Problem:** Physics bodies created synchronously during component Init.

**Solution:** Batch creation or defer to physics thread:
```cpp
class PhysicsWorld {
private:
    std::vector<PendingBodyCreation> m_pendingCreations;
    
public:
    void QueueBodyCreation(/* params */) {
        m_pendingCreations.push_back({/* ... */});
    }
    
    void ProcessPendingCreations() {
        // Batch create all pending bodies
    }
};
```

**Expected Improvement:** Reduced frame spikes when many bodies are created

---

### 14. Physics Update Frequency

**Location:** `src/physics/PhysicsWorld.cpp`

**Problem:** Physics may update at different rate than rendering.

**Solution:** Use fixed timestep with interpolation:
```cpp
class PhysicsWorld {
private:
    float m_accumulator = 0.0f;
    const float m_fixedTimeStep = 1.0f / 60.0f;  // 60 Hz
    
public:
    void Step(float deltaTime) {
        m_accumulator += deltaTime;
        
        while (m_accumulator >= m_fixedTimeStep) {
            m_physicsSystem->Update(m_fixedTimeStep, 1, 1, m_tempAllocator.get(), m_jobSystem.get());
            m_accumulator -= m_fixedTimeStep;
        }
    }
};
```

**Expected Improvement:** More stable physics, better performance

---

## Resource Management Optimizations

### 15. Hot Reloader File System Polling

**Location:** `src/utils/HotReloader.cpp`

**Problem:** Polls file system every frame or at fixed intervals.

**Solution:** Use file system watchers (platform-specific):
```cpp
#ifdef _WIN32
    // Use ReadDirectoryChangesW
#elif __linux__
    // Use inotify
#elif __APPLE__
    // Use FSEvents
#endif
```

**Expected Improvement:** Reduced CPU usage, instant reload detection

---

### 16. Resource Manager Lookup

**Location:** `src/utils/ResourceManager.cpp`

**Problem:** String-based lookups in maps.

**Solution:** Use string interning or hash-based lookups:
```cpp
class ResourceManager {
private:
    // Use string interning
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_shaders;
    // Or use const char* keys with custom hash
    
public:
    std::shared_ptr<Shader> GetShader(const char* name) {  // Avoid string copy
        auto it = m_shaders.find(name);
        return (it != m_shaders.end()) ? it->second : nullptr;
    }
};
```

**Expected Improvement:** Reduced allocations, faster lookups

---

## String and Allocation Optimizations

### 17. Logger String Formatting

**Location:** `src/core/Logger.cpp`

**Problem:** Uses variadic arguments and sprintf-style formatting.

**Solution:** Use fmt library or compile-time format strings:
```cpp
// Use fmt library
Logger::Info("Player health: {}", health);

// Or use constexpr format strings where possible
```

**Expected Improvement:** Faster logging, type-safe formatting

---

### 18. Component Type Name Lookup

**Location:** `src/scene/Component.cpp`

**Problem:**
```cpp
std::string Component::GetName() const {
    return typeid(*this).name();  // May allocate
}
```

**Solution:** Cache type names:
```cpp
class Component {
private:
    static thread_local std::unordered_map<const std::type_info*, std::string> s_typeNameCache;
    
public:
    std::string GetName() const {
        auto& cache = s_typeNameCache;
        auto it = cache.find(&typeid(*this));
        if (it != cache.end()) {
            return it->second;
        }
        
        std::string name = typeid(*this).name();
        cache[&typeid(*this)] = name;
        return name;
    }
};
```

**Expected Improvement:** Reduced allocations in component-heavy scenes

---

## Profiling Recommendations

### Tools to Use

1. **Visual Studio Profiler** - Built-in CPU and memory profiler
2. **Tracy Profiler** - Real-time performance profiler
3. **Intel VTune** - Advanced performance analysis
4. **RenderDoc** - GPU profiling

### Key Metrics to Track

1. **Frame Time** - Target: <16.67ms (60 FPS)
2. **Draw Calls** - Target: <1000 per frame
3. **Memory Allocations** - Track per-frame allocations
4. **Component Lookups** - Time spent in GetComponent
5. **Scene Traversal** - Time spent iterating GameObjects

### Profiling Strategy

1. **Baseline Measurement**
   - Profile current performance
   - Identify top 5 bottlenecks
   - Document frame times and memory usage

2. **Incremental Optimization**
   - Fix one issue at a time
   - Measure improvement
   - Verify no regressions

3. **Continuous Monitoring**
   - Add performance counters
   - Log frame times
   - Alert on performance regressions

---

## Priority Ranking

### High Priority (Do First)
1. ✅ LightManager created every frame (#1)
2. ✅ Active renderable list (#2)
3. ✅ Component lookup optimization (#3)
4. ✅ Frustum culling (#8)

### Medium Priority
5. GameObject name lookup (#10)
6. Instanced rendering (#9)
7. Physics update frequency (#14)
8. Hot reloader file watching (#15)

### Low Priority (Nice to Have)
9. Shader uniform caching (#7)
10. String optimizations (#4, #17, #18)
11. Vector capacity reservation (#5)
12. Destroyed object cleanup (#12)

---

## Implementation Notes

### Testing Strategy
- Profile before and after each optimization
- Use automated benchmarks
- Test with various scene sizes
- Verify correctness (no visual regressions)

### Code Quality
- Maintain readability
- Add comments explaining optimizations
- Use const correctness
- Follow existing code style

### Performance Targets
- **60 FPS** on target hardware
- **<100ms** scene load time
- **<50MB** memory overhead
- **<5%** CPU usage for engine overhead

---

## Additional Resources

- [Optimizing Game Engines](https://www.gdcvault.com/)
- [Data-Oriented Design](https://www.dataorienteddesign.com/)
- [High Performance Game Architecture](https://gameprogrammingpatterns.com/)

---

*Last Updated: Based on codebase analysis*
*Next Review: After implementing high-priority optimizations*

