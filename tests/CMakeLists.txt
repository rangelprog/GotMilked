cmake_minimum_required(VERSION 3.21)

set(GOTMILKED_TEST_SOURCES
    SceneSerializerTests.cpp
    ResourceManagerTests.cpp
    SceneDrawSmokeTest.cpp
    GameResourcesTests.cpp
    TestAssetHelpers.cpp
    LoggerTests.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/GameResources.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/GameSceneHelpers.cpp
    TestMain.cpp
)

if(GM_ENABLE_DEBUG_TOOLS)
    list(APPEND GOTMILKED_TEST_SOURCES
        ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/EditableTerrainComponent.cpp
    )
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

set(GM_GAME_SHADER_DIR "${CMAKE_SOURCE_DIR}/apps/GotMilked/assets/shaders")
string(REPLACE "\\" "/" GM_GAME_SHADER_DIR "${GM_GAME_SHADER_DIR}")
target_compile_definitions(GotMilkedTests PRIVATE "GM_GAME_SHADER_DIR=\"${GM_GAME_SHADER_DIR}\"")

gm_apply_warnings(GotMilkedTests)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(GotMilkedTests)
