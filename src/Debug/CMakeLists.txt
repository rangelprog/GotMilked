set(GM_DEBUG_SOURCES
    GridRenderer.cpp
    TerrainEditingSystem.cpp
)

add_library(gm_debug STATIC ${GM_DEBUG_SOURCES})
target_include_directories(gm_debug
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_debug
    PUBLIC
        gm_core
        gm_rendering
        gm_scene
        gm_utils
        glm::glm
        imgui
)
gm_apply_warnings(gm_debug)

