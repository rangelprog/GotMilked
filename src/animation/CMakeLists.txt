set(GM_ANIMATION_SOURCES
    AnimationClip.cpp
    AnimationPose.cpp
    AnimationPoseEvaluator.cpp
    PoseMath.cpp
    Skeleton.cpp
    SkinnedMeshAsset.cpp
)

add_library(gm_animation STATIC ${GM_ANIMATION_SOURCES})
target_include_directories(gm_animation
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_animation
    PUBLIC
        gm_core
        glm::glm
        nlohmann_json::nlohmann_json
)
gm_apply_warnings(gm_animation)

