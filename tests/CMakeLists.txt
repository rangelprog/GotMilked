cmake_minimum_required(VERSION 3.21)

set(GOTMILKED_TEST_SOURCES
    SceneSerializerTests.cpp
    ResourceManagerTests.cpp
    SceneDrawSmokeTest.cpp
    GameResourcesTests.cpp
    PhysicsWorldTests.cpp
    AnimationPipelineTests.cpp
    GameplayToolingTests.cpp
    TestAssetHelpers.cpp
    LoggerTests.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/CowAnimationController.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/SceneSerializerExtensions.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/GameResources.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/GameSceneHelpers.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/gameplay/FlyCameraController.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/gameplay/CameraRigComponent.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/gameplay/CameraRigSystem.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/gameplay/QuestTriggerComponent.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/gameplay/QuestTriggerSystem.cpp
    TestMain.cpp
)

if(GM_ENABLE_DEBUG_TOOLS)
    list(APPEND GOTMILKED_TEST_SOURCES
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/EditableTerrainComponent.cpp
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/DebugMenu.cpp
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/DebugMenu_Menus.cpp
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/DebugMenu_RecentFiles.cpp
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/DebugMenu_Windows.cpp
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/debug_menu/SceneOutlinerPanel.cpp
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/debug_menu/ContentValidationPanel.cpp
        ${ImGuizmo_SOURCE_DIR}/ImGuizmo.cpp
    )
    if(MSVC)
        set_source_files_properties(${ImGuizmo_SOURCE_DIR}/ImGuizmo.cpp PROPERTIES COMPILE_FLAGS "/wd4245")
    endif()
endif()

add_executable(GotMilkedTests ${GOTMILKED_TEST_SOURCES})
target_link_libraries(GotMilkedTests PRIVATE
    Catch2::Catch2
    GotMilkedEngine
    glfw
    glad
    glm::glm
    imgui
)

target_include_directories(GotMilkedTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src
)
if(GM_ENABLE_DEBUG_TOOLS)
    target_include_directories(GotMilkedTests PRIVATE ${ImGuizmo_SOURCE_DIR})
endif()

set(GM_GAME_SHADER_DIR "${CMAKE_SOURCE_DIR}/apps/GotMilked/assets/shaders")
string(REPLACE "\\" "/" GM_GAME_SHADER_DIR "${GM_GAME_SHADER_DIR}")
target_compile_definitions(GotMilkedTests PRIVATE "GM_GAME_SHADER_DIR=\"${GM_GAME_SHADER_DIR}\"")

set(GM_ASSETS_DIR "${CMAKE_SOURCE_DIR}/apps/GotMilked/assets")
string(REPLACE "\\" "/" GM_ASSETS_DIR "${GM_ASSETS_DIR}")
target_compile_definitions(GotMilkedTests PRIVATE "GM_ASSETS_DIR=\"${GM_ASSETS_DIR}\"")

gm_apply_warnings(GotMilkedTests)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(GotMilkedTests)
