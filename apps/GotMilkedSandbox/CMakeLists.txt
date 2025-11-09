add_executable(GotMilkedSandbox
    src/main.cpp
    src/Camera.cpp
    src/Shader.cpp
    src/Mesh.cpp
    src/Texture.cpp
)

set_target_properties(GotMilkedSandbox PROPERTIES OUTPUT_NAME "GotMilkedSandbox")
gm_apply_warnings(GotMilkedSandbox)
target_link_libraries(GotMilkedSandbox PRIVATE glfw glad glm::glm)

if (APPLE)
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  find_library(IOKIT_LIBRARY IOKit REQUIRED)
  find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
  target_link_libraries(GotMilkedSandbox PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
endif()

# assets define
set(GM_SANDBOX_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
string(REPLACE "\\" "/" GM_SANDBOX_ASSETS_DIR "${GM_SANDBOX_ASSETS_DIR}")
target_compile_definitions(GotMilkedSandbox PRIVATE "GM_ASSETS_DIR=\"${GM_SANDBOX_ASSETS_DIR}\"")
