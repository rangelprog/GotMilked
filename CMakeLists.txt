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

add_subdirectory(src/core)
add_subdirectory(src/rendering)
add_subdirectory(src/animation)
add_subdirectory(src/utils)
add_subdirectory(src/scene)
add_subdirectory(src/physics)
add_subdirectory(src/save)
add_subdirectory(src/prototypes)
add_subdirectory(src/tooling)
add_subdirectory(src/assets)
add_subdirectory(src/runtime)
add_subdirectory(src/content)

if(GM_ENABLE_DEBUG_TOOLS)
    add_subdirectory(src/debug)
endif()

add_library(GotMilkedEngine INTERFACE)
target_include_directories(GotMilkedEngine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(GotMilkedEngine
    INTERFACE
        gm_core
        gm_rendering
        gm_animation
        gm_utils
        gm_scene
        gm_physics
        gm_save
        gm_prototypes
        gm_tooling
        gm_assets
        gm_content
        gm_runtime
)

if(GM_ENABLE_DEBUG_TOOLS)
    target_link_libraries(GotMilkedEngine INTERFACE gm_debug)
endif()

add_custom_target(lock-deps
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/LockDeps.cmake
    COMMENT "Pre-fetching and locking third-party dependencies")

# ---------------------------
# Applications / Executables
# ---------------------------
add_executable(AssimpInspector
    tools/AssimpInspector.cpp
)
target_link_libraries(AssimpInspector PRIVATE assimp::assimp fmt::fmt)
gm_apply_warnings(AssimpInspector)

add_executable(AssimpImporter
    tools/AssimpImporter.cpp
)
target_link_libraries(AssimpImporter
    PRIVATE
        assimp::assimp
        fmt::fmt
        nlohmann_json::nlohmann_json
        glm::glm
        GotMilkedEngine
)
target_include_directories(AssimpImporter PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
gm_apply_warnings(AssimpImporter)

if (GM_BUILD_GAME)
    add_subdirectory(apps/GotMilked)
endif()

if (GM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
