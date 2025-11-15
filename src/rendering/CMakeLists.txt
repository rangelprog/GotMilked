set(GM_RENDERING_SOURCES
    Camera.cpp
    CascadeShadowMap.cpp
    LightManager.cpp
    Material.cpp
    Mesh.cpp
    RenderStateCache.cpp
    Shader.cpp
    Texture.cpp
    stb_image_impl.cpp
)

add_library(gm_rendering STATIC ${GM_RENDERING_SOURCES})
target_include_directories(gm_rendering
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_rendering
    PUBLIC
        gm_core
        glm::glm
        glad
)
gm_apply_warnings(gm_rendering)

