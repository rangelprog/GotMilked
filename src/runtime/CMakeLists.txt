add_library(gm_runtime STATIC GameApp.cpp)
target_include_directories(gm_runtime
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_runtime
    PUBLIC
        gm_core
        gm_scene
        glad
        glfw
)
gm_apply_warnings(gm_runtime)

