add_executable(GotMilkedSandbox
  src/main.cpp
  src/Game.cpp
  src/MeshSpinnerComponent.cpp
  src/SceneSerializerExtensions.cpp
  src/SandboxResources.cpp
  src/SandboxSceneHelpers.cpp
)

set_target_properties(GotMilkedSandbox PROPERTIES OUTPUT_NAME "GotMilkedSandbox")
gm_apply_warnings(GotMilkedSandbox)

target_include_directories(GotMilkedSandbox PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(GotMilkedSandbox PRIVATE GotMilkedEngine)

if (APPLE)
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  find_library(IOKIT_LIBRARY IOKit REQUIRED)
  find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
  target_link_libraries(GotMilkedSandbox PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
endif()

# assets define
set(GM_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
string(REPLACE "\\" "/" GM_ASSETS_DIR "${GM_ASSETS_DIR}")
target_compile_definitions(GotMilkedSandbox PRIVATE "GM_ASSETS_DIR=\"${GM_ASSETS_DIR}\"")
