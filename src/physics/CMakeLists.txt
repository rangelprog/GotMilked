set(GM_PHYSICS_SOURCES
    JoltImpl.cpp
    PhysicsWorld.cpp
    RigidBodyComponent.cpp
)

add_library(gm_physics STATIC ${GM_PHYSICS_SOURCES})
target_include_directories(gm_physics
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_physics
    PUBLIC
        gm_core
        gm_scene
        gm_utils
        glm::glm
        Jolt
)
gm_apply_warnings(gm_physics)

