#pragma once

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

namespace gm::content::SimpleYaml {

/**
 * @brief Parse a limited subset of YAML into JSON.
 *
 * Supported features:
 * - Key/value maps with indentation (2 spaces per level)
 * - Arrays using '- value' syntax
 * - Scalars: strings, quoted strings, integers, floating point, booleans, null
 *
 * @param source Raw YAML text
 * @param out Parsed JSON document
 * @param error Error message on failure
 * @return true on success
 */
bool Parse(const std::string& source, nlohmann::json& out, std::string& error);

/**
 * @brief Load either JSON or YAML file into JSON representation.
 */
bool LoadStructuredFile(const std::filesystem::path& path, nlohmann::json& out, std::string& error);

} // namespace gm::content::SimpleYaml


