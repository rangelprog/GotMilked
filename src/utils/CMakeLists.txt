set(GM_UTILS_SOURCES
    Config.cpp
    CoordinateDisplay.cpp
    FileDialog.cpp
    HotReloader.cpp
    ImGuiManager.cpp
    ObjLoader.cpp
    Profiler.cpp
    ResourceManager.cpp
    ResourceManifest.cpp
    ResourceRegistry.cpp
    ThreadPool.cpp
)

add_library(gm_utils STATIC ${GM_UTILS_SOURCES})
target_include_directories(gm_utils
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_utils
    PUBLIC
        gm_core
        gm_rendering
        gm_animation
        glm::glm
        imgui
        nlohmann_json::nlohmann_json
)
if(WIN32)
    target_link_libraries(gm_utils PUBLIC ole32)
endif()
gm_apply_warnings(gm_utils)

