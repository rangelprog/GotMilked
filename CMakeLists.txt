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

# Warning helper function
function(gm_apply_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ---------------------------
# Dependencies (FetchContent)
# ---------------------------
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

# glad (modern GL loader)
FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG        v0.1.36
)

set(GLAD_PROFILE "core")
set(GLAD_API "gl=4.6")
set(GLAD_GENERATOR "c")
FetchContent_MakeAvailable(glad)

# glm (header-only)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# Jolt Physics (header-only with single implementation file)
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JPH_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(JPH_PROFILE OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  Jolt
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG        v5.0.0
  SOURCE_SUBDIR  Build
)
FetchContent_MakeAvailable(Jolt)

# Dear ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.90.2
)
FetchContent_MakeAvailable(imgui)

# nlohmann/json (header-only JSON library)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# Create imgui library manually since it doesn't have a CMakeLists.txt
if(NOT TARGET imgui)
  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  )
  target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
  )
  target_link_libraries(imgui PUBLIC glfw)
endif()

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
    src/rendering/Mesh.cpp
    src/rendering/Material.cpp
    src/rendering/LightManager.cpp
    src/rendering/stb_image_impl.cpp

    # Scene management
    src/scene/Scene.cpp
    src/scene/SceneManager.cpp
    src/scene/GameObject.cpp
    src/scene/TransformComponent.cpp
    src/scene/MaterialComponent.cpp
    src/scene/LightComponent.cpp
    src/scene/SceneSerializer.cpp

    # Utils
    src/utils/ObjLoader.cpp
    src/utils/ResourceManager.cpp
    src/utils/CoordinateDisplay.cpp
    src/utils/ImGuiManager.cpp
    src/utils/ResourceRegistry.cpp
    src/utils/Config.cpp
    src/utils/HotReloader.cpp

    # Physics
    src/physics/PhysicsWorld.cpp
    src/physics/JoltImpl.cpp

    # Prototypes
    src/prototypes/Primitives.cpp

    # Gameplay
    src/gameplay/FlyCameraController.cpp

    # Save / Tooling
    src/save/SaveManager.cpp
    src/save/SaveSnapshotHelpers.cpp
    src/tooling/Overlay.cpp
)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/include/gm/tooling
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
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tooling
)
target_link_libraries(GotMilkedEngine PUBLIC glfw glad glm::glm imgui Jolt)

# Include directories for dependencies
target_include_directories(GotMilkedEngine PUBLIC ${json_SOURCE_DIR}/include)
# Jolt include directory - FetchContent populates jolt_SOURCE_DIR (lowercase)
target_include_directories(GotMilkedEngine PUBLIC ${jolt_SOURCE_DIR})
# Note: Jolt is header-only with JPH_IMPLEMENTATION in JoltImpl.cpp, so no library linking needed
