set(GM_ASSETS_SOURCES
    AssetCatalog.cpp
    AssetDatabase.cpp
)

add_library(gm_assets STATIC ${GM_ASSETS_SOURCES})
target_include_directories(gm_assets
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_assets
    PUBLIC
        gm_core
        gm_utils
        nlohmann_json::nlohmann_json
)
gm_apply_warnings(gm_assets)

