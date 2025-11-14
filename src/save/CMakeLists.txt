set(GM_SAVE_SOURCES
    SaveDiff.cpp
    SaveManager.cpp
    SaveSnapshotHelpers.cpp
    SaveVersion.cpp
)

add_library(gm_save STATIC ${GM_SAVE_SOURCES})
target_include_directories(gm_save
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_save
    PUBLIC
        gm_core
        gm_scene
        gm_utils
        nlohmann_json::nlohmann_json
)
gm_apply_warnings(gm_save)

