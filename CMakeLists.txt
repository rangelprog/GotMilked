cmake_minimum_required(VERSION 3.21)

# Make sure old subprojects like glad don't trigger legacy policy errors
set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})

project(GotMilked LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to build the sandbox app
option(GM_BUILD_SANDBOX "Build GotMilked Sandbox app" ON)

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

# ---------------------------
# Applications / Executables
# ---------------------------
if (GM_BUILD_SANDBOX)
    add_subdirectory(apps/GotMilkedSandbox)
endif()

add_library(GotMilkedEngine
    src/Camera.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/Mesh.cpp
    src/stb_image_impl.cpp
    src/Scene.cpp
)

target_include_directories(GotMilkedEngine PUBLIC src)
target_link_libraries(GotMilkedEngine PUBLIC glfw glad glm)
