set(GOTMILKED_SOURCES
  src/main.cpp
  src/Game.cpp
  src/SceneSerializerExtensions.cpp
  src/GameResources.cpp
  src/GameSceneHelpers.cpp
)

if(GM_ENABLE_DEBUG_TOOLS)
  list(APPEND GOTMILKED_SOURCES
    src/DebugHudController.cpp
    src/EditableTerrainComponent.cpp
    src/DebugMenu.cpp
    src/DebugMenu_Menus.cpp
    src/DebugMenu_Windows.cpp
    src/DebugMenu_RecentFiles.cpp
    ${ImGuizmo_SOURCE_DIR}/ImGuizmo.cpp
  )
endif()

add_executable(GotMilked ${GOTMILKED_SOURCES})

if(GM_ENABLE_DEBUG_TOOLS AND MSVC)
  set_source_files_properties(${ImGuizmo_SOURCE_DIR}/ImGuizmo.cpp PROPERTIES COMPILE_FLAGS "/wd4245")
endif()

if(GM_ENABLE_DEBUG_TOOLS)
  target_include_directories(GotMilked PRIVATE ${ImGuizmo_SOURCE_DIR})
endif()

set_target_properties(GotMilked PROPERTIES OUTPUT_NAME "GotMilked")
gm_apply_warnings(GotMilked)

target_include_directories(GotMilked PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(GotMilked PRIVATE GotMilkedEngine)

if (APPLE)
  find_library(COCOA_LIBRARY Cocoa REQUIRED)
  find_library(IOKIT_LIBRARY IOKit REQUIRED)
  find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
  target_link_libraries(GotMilked PRIVATE ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
endif()

# config define
set(GM_CONFIG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
string(REPLACE "\\" "/" GM_CONFIG_PATH "${GM_CONFIG_PATH}")
target_compile_definitions(GotMilked PRIVATE "GM_CONFIG_PATH=\"${GM_CONFIG_PATH}\"")
