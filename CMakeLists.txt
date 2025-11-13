cmake_minimum_required(VERSION 3.21)

# Make sure old subprojects like glad don't trigger legacy policy errors
set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})

project(GotMilked LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to build the game app
option(GM_BUILD_GAME "Build GotMilked game app" ON)
option(GM_BUILD_TESTS "Build GotMilked tests" ON)
option(GM_ENABLE_DEBUG_TOOLS "Enable ImGui-based debug tooling (menus, console, terrain editor)" ON)

if(GM_ENABLE_DEBUG_TOOLS)
    add_compile_definitions(GM_DEBUG_TOOLS=1)
else()
    add_compile_definitions(GM_DEBUG_TOOLS=0)
endif()

# Warning helper function
function(gm_apply_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/external")
include(cmake/Dependencies.cmake)

add_library(GotMilkedEngine
    # Core systems
    src/core/Time.cpp
    src/core/Event.cpp
    src/core/GameApp.cpp
    src/core/Input.cpp
    src/core/InputBindings.cpp
    src/core/input/InputSystem.cpp

    # Rendering systems
    src/rendering/Camera.cpp
    src/rendering/Shader.cpp
    src/rendering/Texture.cpp
    src/rendering/RenderStateCache.cpp
    src/rendering/Mesh.cpp
    src/rendering/Material.cpp
    src/rendering/LightManager.cpp
    src/rendering/stb_image_impl.cpp

    # Scene management
    src/scene/Scene.cpp
    src/scene/Scene_GameObject.cpp
    src/scene/PrefabLibrary.cpp
    src/scene/SceneManager.cpp
    src/scene/GameObject.cpp
    src/scene/Component.cpp
    src/scene/TransformComponent.cpp
    src/scene/MaterialComponent.cpp
    src/scene/LightComponent.cpp
    src/scene/SceneSerializer.cpp
    src/scene/ComponentFactory.cpp
    src/scene/StaticMeshComponent.cpp

    # Utils
    src/utils/ObjLoader.cpp
    src/utils/ResourceManager.cpp
    src/utils/ResourceManifest.cpp
    src/utils/CoordinateDisplay.cpp
    src/utils/ImGuiManager.cpp
    src/utils/ResourceRegistry.cpp
    src/utils/Config.cpp
    src/utils/HotReloader.cpp
    src/utils/FileDialog.cpp
    src/utils/ThreadPool.cpp
    src/utils/Profiler.cpp

    # Physics
    src/physics/PhysicsWorld.cpp
    src/physics/JoltImpl.cpp
    src/physics/RigidBodyComponent.cpp

    # Prototypes
    src/prototypes/Primitives.cpp

    # Gameplay
    src/gameplay/FlyCameraController.cpp
    src/gameplay/CameraRigComponent.cpp
    src/gameplay/CameraRigSystem.cpp
    src/gameplay/QuestTriggerComponent.cpp
    src/gameplay/QuestTriggerSystem.cpp

    # Save / Tooling
    src/save/SaveManager.cpp
    src/save/SaveSnapshotHelpers.cpp
    src/save/SaveVersion.cpp
    src/save/SaveDiff.cpp
    src/tooling/Overlay.cpp

    # Assets
    src/assets/AssetCatalog.cpp
    src/assets/AssetDatabase.cpp
)

if(GM_ENABLE_DEBUG_TOOLS)
    target_sources(GotMilkedEngine PRIVATE
        src/tooling/DebugConsole.cpp
        src/debug/GridRenderer.cpp
        src/debug/TerrainEditingSystem.cpp
    )
endif()

target_include_directories(GotMilkedEngine 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/core
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/rendering
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/scene
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/physics
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/prototypes
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/gameplay
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/save
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/assets
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/tooling
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/debug
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rendering
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scene
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
        ${CMAKE_CURRENT_SOURCE_DIR}/src/physics
        ${CMAKE_CURRENT_SOURCE_DIR}/src/prototypes
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gameplay
        ${CMAKE_CURRENT_SOURCE_DIR}/src/save
        ${CMAKE_CURRENT_SOURCE_DIR}/src/assets
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooling
        ${CMAKE_CURRENT_SOURCE_DIR}/src/debug
)
target_link_libraries(GotMilkedEngine PUBLIC glfw glad glm::glm imgui Jolt)
target_link_libraries(GotMilkedEngine PUBLIC fmt::fmt nlohmann_json::nlohmann_json)
gm_apply_warnings(GotMilkedEngine)

# Windows-specific libraries for user documents path resolution
if(WIN32)
    target_link_libraries(GotMilkedEngine PRIVATE ole32)
endif()

target_include_directories(GotMilkedEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(GotMilkedEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_custom_target(lock-deps
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/LockDeps.cmake
    COMMENT "Pre-fetching and locking third-party dependencies")

# ---------------------------
# Applications / Executables
# ---------------------------
if (GM_BUILD_GAME)
    add_subdirectory(apps/GotMilked)
endif()

if (GM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
