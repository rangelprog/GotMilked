add_library(gm_prototypes STATIC Primitives.cpp)
target_include_directories(gm_prototypes
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_prototypes
    PUBLIC
        gm_core
        gm_rendering
        gm_scene
        glm::glm
)
gm_apply_warnings(gm_prototypes)

