# Profiling Recommendations

This document provides comprehensive profiling recommendations for the GotMilked engine, including tools, techniques, metrics, and best practices.

## Table of Contents

1. [Profiling Tools](#profiling-tools)
2. [Key Metrics to Track](#key-metrics-to-track)
3. [Profiling Strategy](#profiling-strategy)
4. [CPU Profiling](#cpu-profiling)
5. [Memory Profiling](#memory-profiling)
6. [GPU Profiling](#gpu-profiling)
7. [Frame Analysis](#frame-analysis)
8. [Performance Counters](#performance-counters)
9. [Automated Profiling](#automated-profiling)
10. [Interpreting Results](#interpreting-results)

---

## Profiling Tools

### CPU Profiling Tools

#### 1. Visual Studio Profiler (Recommended for Windows)
**Best For:** Integrated development, detailed call stacks, memory profiling

**Setup:**
```cpp
// Enable profiling in Visual Studio:
// Debug → Performance Profiler → CPU Usage
// Or: Debug → Start Diagnostic Tools Without Debugging
```

**Features:**
- Hot path identification
- Call tree analysis
- Function-level timing
- Memory allocation tracking
- Thread analysis

**Usage:**
1. Open project in Visual Studio
2. Debug → Performance Profiler
3. Select "CPU Usage"
4. Click "Start"
5. Run game for 30-60 seconds
6. Stop and analyze results

---

#### 2. Tracy Profiler (Recommended for Real-Time)
**Best For:** Real-time performance monitoring, frame-by-frame analysis

**Setup:**
```cpp
// Add to CMakeLists.txt
find_package(Tracy CONFIG REQUIRED)

// In code
#include <Tracy.hpp>

// Mark zones
ZoneScoped;  // Automatic zone name from function
ZoneScopedN("CustomZoneName");
ZoneNamedN(zone, "GameUpdate", true);
```

**Features:**
- Real-time frame graphs
- Zone profiling
- Memory tracking
- GPU profiling
- Thread visualization
- Minimal overhead (~1-2%)

**Usage:**
1. Build with Tracy support
2. Run game
3. Open Tracy client
4. Connect to running game
5. View real-time performance data

---

#### 3. Intel VTune Profiler
**Best For:** Advanced CPU analysis, hardware event sampling

**Features:**
- Hardware event sampling
- Cache miss analysis
- Branch prediction analysis
- CPU pipeline analysis
- Advanced optimization hints

**Usage:**
1. Install Intel VTune
2. Create new project
3. Configure target executable
4. Run analysis
5. Review hotspots and bottlenecks

---

#### 4. AMD uProf (For AMD CPUs)
**Best For:** AMD processor-specific optimizations

**Features:**
- AMD-specific hardware counters
- Cache hierarchy analysis
- Power consumption analysis

---

### Memory Profiling Tools

#### 1. Visual Studio Diagnostic Tools
**Best For:** Integrated memory profiling

**Features:**
- Allocation tracking
- Memory leak detection
- Heap snapshots
- Object lifetime analysis

**Usage:**
1. Debug → Performance Profiler
2. Select "Memory Usage"
3. Take snapshots at key points
4. Compare snapshots

---

#### 2. Valgrind (Linux)
**Best For:** Linux memory profiling, leak detection

**Features:**
- Memory leak detection
- Invalid memory access detection
- Heap profiling

**Usage:**
```bash
valgrind --leak-check=full --show-leak-kinds=all ./GotMilked
```

---

#### 3. Application Verifier (Windows)
**Best For:** Windows-specific memory issues

**Features:**
- Heap corruption detection
- Handle leak detection
- Lock contention detection

---

### GPU Profiling Tools

#### 1. RenderDoc (Recommended)
**Best For:** Frame capture, draw call analysis, GPU timing

**Features:**
- Frame capture and replay
- Draw call visualization
- Texture/shader inspection
- GPU timing
- API call logging

**Usage:**
1. Launch RenderDoc
2. Inject into game process
3. Capture frame
4. Analyze draw calls, textures, shaders

---

#### 2. NVIDIA Nsight Graphics
**Best For:** NVIDIA GPU profiling, shader analysis

**Features:**
- Frame capture
- Shader debugging
- GPU timing
- API trace
- Performance counters

---

#### 3. AMD Radeon GPU Profiler
**Best For:** AMD GPU profiling

**Features:**
- Frame analysis
- Shader profiling
- GPU timing
- Memory bandwidth analysis

---

## Key Metrics to Track

### Frame Time Metrics

```cpp
// In Game::Update()
class FrameTimer {
public:
    void StartFrame() {
        m_frameStart = std::chrono::high_resolution_clock::now();
    }
    
    void EndFrame() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - m_frameStart);
        
        m_frameTime = duration.count() / 1000.0f;  // Convert to ms
        m_frameTimes.push_back(m_frameTime);
        
        // Keep only last 60 frames
        if (m_frameTimes.size() > 60) {
            m_frameTimes.erase(m_frameTimes.begin());
        }
        
        CalculateStats();
    }
    
    float GetAverageFrameTime() const { return m_averageFrameTime; }
    float GetMinFrameTime() const { return m_minFrameTime; }
    float GetMaxFrameTime() const { return m_maxFrameTime; }
    float GetFrameTimeVariance() const { return m_variance; }
    
private:
    void CalculateStats() {
        if (m_frameTimes.empty()) return;
        
        float sum = 0.0f;
        for (float ft : m_frameTimes) {
            sum += ft;
        }
        m_averageFrameTime = sum / m_frameTimes.size();
        
        m_minFrameTime = *std::min_element(m_frameTimes.begin(), m_frameTimes.end());
        m_maxFrameTime = *std::max_element(m_frameTimes.begin(), m_frameTimes.end());
        
        // Calculate variance
        float varianceSum = 0.0f;
        for (float ft : m_frameTimes) {
            float diff = ft - m_averageFrameTime;
            varianceSum += diff * diff;
        }
        m_variance = varianceSum / m_frameTimes.size();
    }
    
    std::chrono::high_resolution_clock::time_point m_frameStart;
    float m_frameTime = 0.0f;
    std::vector<float> m_frameTimes;
    float m_averageFrameTime = 0.0f;
    float m_minFrameTime = 0.0f;
    float m_maxFrameTime = 0.0f;
    float m_variance = 0.0f;
};
```

**Target Values:**
- **60 FPS:** Average < 16.67ms, Max < 20ms
- **120 FPS:** Average < 8.33ms, Max < 10ms
- **Variance:** < 2ms (consistent frame times)

---

### CPU Usage Metrics

```cpp
// Track CPU usage per system
class SystemProfiler {
public:
    struct SystemTiming {
        std::string name;
        float totalTime = 0.0f;
        float averageTime = 0.0f;
        int callCount = 0;
    };
    
    void StartSystem(const std::string& name) {
        m_currentSystem = name;
        m_startTime = std::chrono::high_resolution_clock::now();
    }
    
    void EndSystem() {
        if (m_currentSystem.empty()) return;
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - m_startTime);
        float timeMs = duration.count() / 1000.0f;
        
        auto& timing = m_systemTimings[m_currentSystem];
        timing.name = m_currentSystem;
        timing.totalTime += timeMs;
        timing.callCount++;
        timing.averageTime = timing.totalTime / timing.callCount;
        
        m_currentSystem.clear();
    }
    
    void LogResults() {
        core::Logger::Info("=== System Performance ===");
        for (const auto& [name, timing] : m_systemTimings) {
        core::Logger::Info("{}: {:.2f}ms (avg), {} calls",
                              timing.name,
                              timing.averageTime,
                              timing.callCount);
        }
    }
    
private:
    std::string m_currentSystem;
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::unordered_map<std::string, SystemTiming> m_systemTimings;
};

// Usage
SystemProfiler profiler;
profiler.StartSystem("Physics");
physics.Step(dt);
profiler.EndSystem();

profiler.StartSystem("Rendering");
scene.Draw(...);
profiler.EndSystem();
```

**Target Values:**
- **Physics:** < 2ms per frame
- **Rendering:** < 10ms per frame
- **Input:** < 0.1ms per frame
- **Game Logic:** < 1ms per frame
- **Total CPU:** < 80% of frame budget

---

### Memory Metrics

```cpp
// Track memory usage
class MemoryProfiler {
public:
    struct MemorySnapshot {
        size_t totalAllocated = 0;
        size_t peakAllocated = 0;
        size_t currentAllocated = 0;
        size_t allocationCount = 0;
        size_t deallocationCount = 0;
    };
    
    void TakeSnapshot(const std::string& label) {
        MemorySnapshot snapshot;
        // Use platform-specific APIs to get memory info
        #ifdef _WIN32
            PROCESS_MEMORY_COUNTERS_EX pmc;
            GetProcessMemoryInfo(GetCurrentProcess(), 
                                (PROCESS_MEMORY_COUNTERS*)&pmc, 
                                sizeof(pmc));
            snapshot.currentAllocated = pmc.WorkingSetSize;
            snapshot.peakAllocated = pmc.PeakWorkingSetSize;
        #elif __linux__
            // Read from /proc/self/status
        #endif
        
        m_snapshots[label] = snapshot;
    }
    
    void CompareSnapshots(const std::string& before, const std::string& after) {
        auto& beforeSnap = m_snapshots[before];
        auto& afterSnap = m_snapshots[after];
        
        size_t diff = afterSnap.currentAllocated - beforeSnap.currentAllocated;
        core::Logger::Info("Memory change: {} -> {}: {} bytes ({:.2f} MB)",
                          before, after, diff, diff / 1024.0f / 1024.0f);
    }
    
private:
    std::unordered_map<std::string, MemorySnapshot> m_snapshots;
};
```

**Target Values:**
- **Base Memory:** < 100MB
- **Peak Memory:** < 500MB
- **Memory Growth:** < 1MB per minute
- **Leaks:** 0 (no memory leaks)

---

### Draw Call Metrics

```cpp
// Track rendering statistics
class RenderStats {
public:
    void Reset() {
        m_drawCalls = 0;
        m_triangles = 0;
        m_vertices = 0;
        m_textureBinds = 0;
        m_shaderBinds = 0;
    }
    
    void IncrementDrawCalls() { m_drawCalls++; }
    void AddTriangles(int count) { m_triangles += count; }
    void AddVertices(int count) { m_vertices += count; }
    void IncrementTextureBinds() { m_textureBinds++; }
    void IncrementShaderBinds() { m_shaderBinds++; }
    
    void LogStats() {
        core::Logger::Info("=== Render Stats ===");
        core::Logger::Info("Draw Calls: {}", m_drawCalls);
        core::Logger::Info("Triangles: {}", m_triangles);
        core::Logger::Info("Vertices: {}", m_vertices);
        core::Logger::Info("Texture Binds: {}", m_textureBinds);
        core::Logger::Info("Shader Binds: {}", m_shaderBinds);
    }
    
private:
    int m_drawCalls = 0;
    int m_triangles = 0;
    int m_vertices = 0;
    int m_textureBinds = 0;
    int m_shaderBinds = 0;
};
```

**Target Values:**
- **Draw Calls:** < 1000 per frame
- **Triangles:** < 1M per frame
- **Texture Binds:** < 100 per frame
- **Shader Binds:** < 50 per frame

---

## Profiling Strategy

### 1. Baseline Measurement

**Before making any changes:**
```cpp
// Create baseline profile
void CreateBaselineProfile() {
    // Run game for 60 seconds
    // Record:
    // - Average frame time
    // - Peak frame time
    // - CPU usage per system
    // - Memory usage
    // - Draw calls
    // - Hotspots from profiler
}
```

**Document:**
- Hardware specifications
- Scene complexity
- Frame time statistics
- Top 10 hotspots
- Memory usage patterns

---

### 2. Incremental Profiling

**After each optimization:**
```cpp
// Compare before/after
void CompareProfiles(const Profile& before, const Profile& after) {
    float frameTimeImprovement = 
        (before.averageFrameTime - after.averageFrameTime) / 
        before.averageFrameTime * 100.0f;
    
    core::Logger::Info("Frame time improvement: {:.2f}%", frameTimeImprovement);
    
    // Verify no regressions
    assert(after.averageFrameTime <= before.averageFrameTime * 1.05f);  // Allow 5% variance
}
```

**Check:**
- Frame time improvement
- No regressions in other systems
- Memory usage hasn't increased unexpectedly
- Visual correctness (no rendering bugs)

---

### 3. Continuous Monitoring

**Add performance counters to game:**
```cpp
// In-game performance overlay
class PerformanceOverlay {
public:
    void Render() {
        ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        // Frame time graph
        ImGui::PlotLines("Frame Time (ms)", m_frameTimeHistory.data(), 
                        m_frameTimeHistory.size(), 0, nullptr, 0.0f, 33.0f);
        
        // Current stats
        ImGui::Text("FPS: %.1f", 1000.0f / m_currentFrameTime);
        ImGui::Text("Frame Time: %.2f ms", m_currentFrameTime);
        ImGui::Text("Min: %.2f ms", m_minFrameTime);
        ImGui::Text("Max: %.2f ms", m_maxFrameTime);
        
        // System breakdown
        ImGui::Separator();
        ImGui::Text("System Timings:");
        for (const auto& [name, time] : m_systemTimings) {
            ImGui::Text("%s: %.2f ms", name.c_str(), time);
        }
        
        ImGui::End();
    }
    
private:
    std::vector<float> m_frameTimeHistory;
    float m_currentFrameTime = 0.0f;
    float m_minFrameTime = 0.0f;
    float m_maxFrameTime = 0.0f;
    std::unordered_map<std::string, float> m_systemTimings;
};
```

---

## CPU Profiling

### Zone Profiling

```cpp
// Add zone markers throughout codebase
class ZoneProfiler {
public:
    ZoneProfiler(const char* name) : m_name(name) {
        m_start = std::chrono::high_resolution_clock::now();
    }
    
    ~ZoneProfiler() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - m_start);
        float timeMs = duration.count() / 1000.0f;
        
        // Log or store timing
        if (timeMs > 1.0f) {  // Only log if > 1ms
            core::Logger::Debug("[Zone] {}: {:.2f} ms", m_name, timeMs);
        }
    }
    
private:
    const char* m_name;
    std::chrono::high_resolution_clock::time_point m_start;
};

// Usage
void Scene::Draw(...) {
    ZoneProfiler zone("Scene::Draw");
    
    {
        ZoneProfiler zone2("UpdateActiveLists");
        UpdateActiveLists();
    }
    
    {
        ZoneProfiler zone3("CollectLights");
        m_lightManager.CollectLights(gameObjects);
    }
    
    {
        ZoneProfiler zone4("ApplyLights");
        m_lightManager.ApplyLights(shader, cam.Position());
    }
}
```

---

### Hotspot Identification

**What to look for:**
1. Functions taking > 5% of frame time
2. Functions called > 1000 times per frame
3. Functions with high variance (inconsistent timing)
4. Functions in critical paths (called every frame)

**Example hotspots to investigate:**
- `GameObject::GetComponent()` - Should be O(1)
- `Scene::Draw()` - Should be optimized
- `LightManager::ApplyLights()` - Many uniform updates
- `PhysicsWorld::Step()` - Physics simulation

---

## Memory Profiling

### Allocation Tracking

```cpp
// Custom allocator with tracking
class TrackedAllocator {
public:
    static void* Allocate(size_t size, const char* file, int line) {
        void* ptr = malloc(size);
        
        std::lock_guard<std::mutex> lock(s_mutex);
        s_allocations[ptr] = {size, file, line};
        s_totalAllocated += size;
        s_allocationCount++;
        
        return ptr;
    }
    
    static void Deallocate(void* ptr) {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_allocations.find(ptr);
        if (it != s_allocations.end()) {
            s_totalAllocated -= it->second.size;
            s_allocations.erase(it);
        }
        
        free(ptr);
    }
    
    static void PrintStats() {
        std::lock_guard<std::mutex> lock(s_mutex);
        core::Logger::Info("=== Memory Stats ===");
        core::Logger::Info("Total Allocated: {:.2f} MB", s_totalAllocated / 1024.0f / 1024.0f);
        core::Logger::Info("Allocation Count: {}", s_allocationCount);
        core::Logger::Info("Active Allocations: {}", s_allocations.size());
    }
    
private:
    struct AllocationInfo {
        size_t size;
        const char* file;
        int line;
    };
    
    static std::mutex s_mutex;
    static std::unordered_map<void*, AllocationInfo> s_allocations;
    static size_t s_totalAllocated;
    static size_t s_allocationCount;
};

// Override new/delete
#define new new(__FILE__, __LINE__)
void* operator new(size_t size, const char* file, int line) {
    return TrackedAllocator::Allocate(size, file, line);
}
```

---

### Leak Detection

**Check for:**
1. Memory that grows continuously
2. Objects not being destroyed
3. Circular references (shared_ptr cycles)
4. Missing cleanup in destructors

**Tools:**
- Visual Studio Diagnostic Tools
- Valgrind (Linux)
- Application Verifier (Windows)

---

## GPU Profiling

### Draw Call Analysis

**Use RenderDoc to:**
1. Capture a frame
2. Count draw calls
3. Identify redundant state changes
4. Find overdraw issues
5. Analyze shader performance

**Optimize:**
- Batch similar objects (instanced rendering)
- Reduce texture binds
- Minimize shader switches
- Use texture atlases

---

### GPU Timing

```cpp
// GPU timing queries
class GPUTimer {
public:
    void Start() {
        glGenQueries(1, &m_query);
        glBeginQuery(GL_TIME_ELAPSED, m_query);
    }
    
    void End() {
        glEndQuery(GL_TIME_ELAPSED);
        
        GLuint64 timeElapsed = 0;
        glGetQueryObjectui64v(m_query, GL_QUERY_RESULT, &timeElapsed);
        float timeMs = timeElapsed / 1000000.0f;  // Convert to ms
        
        core::Logger::Info("GPU Time: {:.2f} ms", timeMs);
        
        glDeleteQueries(1, &m_query);
    }
    
private:
    GLuint m_query = 0;
};
```

---

## Frame Analysis

### Frame Breakdown

```cpp
// Detailed frame timing
struct FrameTiming {
    float inputTime = 0.0f;
    float physicsTime = 0.0f;
    float gameLogicTime = 0.0f;
    float renderingTime = 0.0f;
    float presentTime = 0.0f;
    float totalTime = 0.0f;
};

void Game::Update(float dt) {
    FrameTiming timing;
    auto frameStart = std::chrono::high_resolution_clock::now();
    
    // Input
    auto start = std::chrono::high_resolution_clock::now();
    ProcessInput();
    timing.inputTime = GetElapsedMs(start);
    
    // Physics
    start = std::chrono::high_resolution_clock::now();
    m_physics.Step(dt);
    timing.physicsTime = GetElapsedMs(start);
    
    // Game Logic
    start = std::chrono::high_resolution_clock::now();
    m_scene->UpdateGameObjects(dt);
    timing.gameLogicTime = GetElapsedMs(start);
    
    // Rendering
    start = std::chrono::high_resolution_clock::now();
    Render();
    timing.renderingTime = GetElapsedMs(start);
    
    // Present
    start = std::chrono::high_resolution_clock::now();
    glfwSwapBuffers(m_window);
    timing.presentTime = GetElapsedMs(start);
    
    timing.totalTime = GetElapsedMs(frameStart);
    
    // Log if frame time is high
    if (timing.totalTime > 20.0f) {
        LogFrameTiming(timing);
    }
}
```

---

## Performance Counters

### Built-in Counters

```cpp
// Add performance counters to engine
class PerformanceCounters {
public:
    static PerformanceCounters& Instance() {
        static PerformanceCounters instance;
        return instance;
    }
    
    void IncrementCounter(const std::string& name, int value = 1) {
        m_counters[name] += value;
    }
    
    void SetCounter(const std::string& name, int value) {
        m_counters[name] = value;
    }
    
    int GetCounter(const std::string& name) const {
        auto it = m_counters.find(name);
        return (it != m_counters.end()) ? it->second : 0;
    }
    
    void Reset() {
        m_counters.clear();
    }
    
    void LogCounters() {
        core::Logger::Info("=== Performance Counters ===");
        for (const auto& [name, value] : m_counters) {
            core::Logger::Info("{}: {}", name.c_str(), value);
        }
    }
    
private:
    std::unordered_map<std::string, int> m_counters;
};

// Usage
PerformanceCounters::Instance().IncrementCounter("DrawCalls");
PerformanceCounters::Instance().IncrementCounter("GameObjectsRendered");
PerformanceCounters::Instance().SetCounter("ActiveGameObjects", activeCount);
```

**Key Counters to Track:**
- Draw calls
- GameObjects rendered
- Active GameObjects
- Component lookups
- Physics bodies
- Lights active
- Texture binds
- Shader binds

---

## Automated Profiling

### Continuous Integration Profiling

```cpp
// Automated performance tests
class PerformanceTest {
public:
    struct TestResult {
        float averageFrameTime = 0.0f;
        float minFrameTime = 0.0f;
        float maxFrameTime = 0.0f;
        size_t memoryUsage = 0;
        int drawCalls = 0;
    };
    
    TestResult RunTest(const std::string& sceneName, int frameCount = 1000) {
        LoadScene(sceneName);
        
        TestResult result;
        std::vector<float> frameTimes;
        frameTimes.reserve(frameCount);
        
        for (int i = 0; i < frameCount; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            
            Update(1.0f / 60.0f);
            Render();
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end - start);
            float frameTime = duration.count() / 1000.0f;
            frameTimes.push_back(frameTime);
        }
        
        // Calculate statistics
        float sum = 0.0f;
        for (float ft : frameTimes) {
            sum += ft;
        }
        result.averageFrameTime = sum / frameTimes.size();
        result.minFrameTime = *std::min_element(frameTimes.begin(), frameTimes.end());
        result.maxFrameTime = *std::max_element(frameTimes.begin(), frameTimes.end());
        
        return result;
    }
    
    bool CompareResults(const TestResult& baseline, const TestResult& current) {
        // Fail if performance regressed by more than 5%
        if (current.averageFrameTime > baseline.averageFrameTime * 1.05f) {
            core::Logger::Error("Performance regression detected!");
            core::Logger::Error("Baseline: {:.2f} ms, Current: {:.2f} ms",
                               baseline.averageFrameTime, current.averageFrameTime);
            return false;
        }
        return true;
    }
};
```

---

## Interpreting Results

### Identifying Bottlenecks

**Red Flags:**
1. **Single function > 30% of frame time**
   - Likely bottleneck
   - Investigate optimization opportunities

2. **Many small functions > 1% each**
   - Consider batching or caching

3. **High variance in frame time**
   - Inconsistent performance
   - Look for conditional paths or allocations

4. **Memory continuously growing**
   - Potential memory leak
   - Check object lifecycle

5. **High draw call count**
   - Consider instanced rendering
   - Batch similar objects

---

### Optimization Priorities

**Based on profiling results:**

1. **If CPU-bound:**
   - Focus on CPU optimizations
   - Reduce computation
   - Cache expensive operations
   - Optimize hot paths

2. **If GPU-bound:**
   - Reduce draw calls
   - Optimize shaders
   - Reduce overdraw
   - Use LOD systems

3. **If Memory-bound:**
   - Reduce allocations
   - Use object pooling
   - Optimize data structures
   - Reduce memory footprint

---

## Best Practices

### 1. Profile Release Builds

**Debug builds are misleading:**
- Slower execution
- Different optimization levels
- Additional checks enabled

**Always profile optimized builds:**
```cmake
# CMakeLists.txt
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
```

---

### 2. Profile Representative Scenes

**Test with:**
- Typical gameplay scenarios
- Worst-case scenarios (many objects)
- Edge cases (stress tests)

---

### 3. Multiple Runs

**Run multiple times:**
- System variance
- Cache warm-up effects
- Background process interference

**Take average of 5-10 runs**

---

### 4. Document Profiling Setup

**Record:**
- Hardware specifications
- Software versions
- Profiling tool settings
- Scene complexity
- Test duration

---

### 5. Profile Before and After

**Always:**
- Create baseline before optimization
- Profile after optimization
- Compare results
- Verify improvements

---

## Recommended Profiling Workflow

### Daily Development

1. **Use in-game performance overlay**
   - Monitor frame time
   - Watch for spikes
   - Check system timings

2. **Quick profiling (5 minutes)**
   - Visual Studio Diagnostic Tools
   - Identify obvious hotspots
   - Quick fixes

### Weekly Review

1. **Detailed profiling (30 minutes)**
   - Tracy Profiler session
   - Identify optimization opportunities
   - Plan next optimizations

2. **Memory profiling**
   - Check for leaks
   - Monitor memory growth
   - Optimize allocations

### Before Release

1. **Comprehensive profiling (2 hours)**
   - All scenes
   - All hardware targets
   - Performance regression tests
   - Memory leak detection

2. **GPU profiling**
   - RenderDoc captures
   - Draw call optimization
   - Shader optimization

---

## Tools Integration

### Tracy Integration Example

```cpp
// In Game.cpp
#include <Tracy.hpp>

void Game::Update(float dt) {
    ZoneScopedN("Game::Update");
    
    {
        ZoneScopedN("Input");
        ProcessInput();
    }
    
    {
        ZoneScopedN("Physics");
        m_physics.Step(dt);
    }
    
    {
        ZoneScopedN("SceneUpdate");
        m_scene->UpdateGameObjects(dt);
    }
}

void Game::Render() {
    ZoneScopedN("Game::Render");
    
    {
        ZoneScopedN("SceneDraw");
        m_scene->Draw(*m_shader, *m_camera, width, height, fov);
    }
    
    {
        ZoneScopedN("ImGui");
        m_imgui->Render();
    }
    
    FrameMark;  // Mark frame boundary
}
```

---

## Performance Budgets

### Frame Time Budget (60 FPS target)

| System | Budget | Target |
|--------|--------|--------|
| Input | 0.1ms | < 0.1ms |
| Physics | 2.0ms | < 2.0ms |
| Game Logic | 1.0ms | < 1.0ms |
| Rendering | 10.0ms | < 10.0ms |
| ImGui | 1.0ms | < 1.0ms |
| Present | 1.0ms | < 1.0ms |
| **Total** | **16.67ms** | **< 16.67ms** |

### Memory Budget

| Category | Budget | Target |
|----------|--------|--------|
| Base Engine | 50MB | < 50MB |
| Scene Data | 100MB | < 100MB |
| Textures | 200MB | < 200MB |
| Meshes | 100MB | < 100MB |
| **Total** | **450MB** | **< 500MB** |

---

## Conclusion

**Key Takeaways:**

1. **Profile regularly** - Don't wait for performance issues
2. **Use multiple tools** - Different tools reveal different insights
3. **Measure before optimizing** - Know what to optimize
4. **Verify improvements** - Ensure optimizations actually help
5. **Automate testing** - Catch regressions early

**Next Steps:**

1. Set up Tracy Profiler
2. Add performance counters
3. Create baseline profiles
4. Implement continuous monitoring
5. Schedule regular profiling sessions

---

*Last Updated: Based on current profiling needs*  
*Next Review: After setting up automated profiling*

