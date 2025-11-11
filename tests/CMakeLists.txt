cmake_minimum_required(VERSION 3.21)

add_executable(GotMilkedTests
    SceneSerializerTests.cpp
    ResourceManagerTests.cpp
    SceneDrawSmokeTest.cpp
    TestAssetHelpers.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilkedSandbox/src/MeshSpinnerComponent.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilkedSandbox/src/SandboxResources.cpp
    ${CMAKE_SOURCE_DIR}/apps/GotMilkedSandbox/src/SandboxSceneHelpers.cpp
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
    ${CMAKE_SOURCE_DIR}/apps/GotMilkedSandbox/src
)

gm_apply_warnings(GotMilkedTests)

add_test(NAME SceneSerializer.RoundTrip COMMAND GotMilkedTests)
add_test(NAME Scene.Draw COMMAND GotMilkedTests --scene-draw)

