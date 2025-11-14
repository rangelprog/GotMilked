set(GM_TOOLING_SOURCES
    Overlay.cpp
)

if(GM_ENABLE_DEBUG_TOOLS)
    list(APPEND GM_TOOLING_SOURCES DebugConsole.cpp)
endif()

add_library(gm_tooling STATIC ${GM_TOOLING_SOURCES})
target_include_directories(gm_tooling
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_tooling
    PUBLIC
        gm_core
        gm_physics
        gm_utils
        imgui
)
gm_apply_warnings(gm_tooling)

