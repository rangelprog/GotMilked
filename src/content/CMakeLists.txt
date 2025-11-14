set(GM_CONTENT_SOURCES
    ContentSchemaRegistry.cpp
    ContentDatabase.cpp
    SimpleYaml.cpp
)

add_library(gm_content STATIC ${GM_CONTENT_SOURCES})
target_include_directories(gm_content
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(gm_content
    PUBLIC
        gm_core
        gm_utils
        gm_assets
        nlohmann_json::nlohmann_json
)
gm_apply_warnings(gm_content)


