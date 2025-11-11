cmake_minimum_required(VERSION 3.21)

add_executable(GotMilkedTests
    SceneSerializerTests.cpp
    ResourceManagerTests.cpp
    SceneDrawSmokeTest.cpp
    GameResourcesTests.cpp
    TestAssetHelpers.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/GameResources.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/GameSceneHelpers.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilked/src/EditableTerrainComponent.cpp
    TestMain.cpp
)
target_link_libraries(GotMilkedTests PRIVATE
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

add_test(NAME SceneSerializer.RoundTrip COMMAND GotMilkedTests)
add_test(NAME Scene.Draw COMMAND GotMilkedTests --scene-draw)

