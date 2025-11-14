set(GM_CORE_SOURCES
    Event.cpp
    Input.cpp
    InputBindings.cpp
    Time.cpp
    input/InputSystem.cpp
)

add_library(gm_core STATIC ${GM_CORE_SOURCES})
target_include_directories(gm_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_core
    PUBLIC
        fmt::fmt
        glfw
        glm::glm
)
gm_apply_warnings(gm_core)

