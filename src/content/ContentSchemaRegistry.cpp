#include "gm/content/ContentSchemaRegistry.hpp"

#include "gm/content/SimpleYaml.hpp"
#include "gm/core/Logger.hpp"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <queue>
#include <fmt/format.h>

namespace gm::content {

namespace {

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ParseFieldNode(const nlohmann::json& node, SchemaField& outField, std::string& error) {
    if (!node.contains("name") || !node["name"].is_string()) {
        error = "field is missing 'name'";
        return false;
    }
    if (!node.contains("type") || !node["type"].is_string()) {
        error = fmt::format("field '{}' missing 'type'", node["name"].is_string() ? node["name"].get<std::string>() : "<unknown>");
        return false;
    }

    outField.name = node["name"].get<std::string>();
    const std::string type = ToLowerCopy(node["type"].get<std::string>());
    if (type == "string") {
        outField.kind = FieldKind::String;
    } else if (type == "integer") {
        outField.kind = FieldKind::Integer;
    } else if (type == "number" || type == "float" || type == "double") {
        outField.kind = FieldKind::Number;
    } else if (type == "boolean" || type == "bool") {
        outField.kind = FieldKind::Boolean;
    } else if (type == "object") {
        outField.kind = FieldKind::Object;
    } else if (type == "array" || type == "list") {
        outField.kind = FieldKind::Array;
    } else {
        error = fmt::format("field '{}' has unsupported type '{}'", outField.name, type);
        return false;
    }

    outField.required = node.value("required", false);

    if (node.contains("enum") && node["enum"].is_array()) {
        for (const auto& entry : node["enum"]) {
            if (entry.is_string()) {
                outField.enumValues.push_back(entry.get<std::string>());
            }
        }
    }
    if (node.contains("minimum") && node["minimum"].is_number()) {
        outField.minimum = node["minimum"].get<double>();
    }
    if (node.contains("maximum") && node["maximum"].is_number()) {
        outField.maximum = node["maximum"].get<double>();
    }
    if (node.contains("minLength") && node["minLength"].is_number_unsigned()) {
        outField.minLength = node["minLength"].get<std::size_t>();
    }
    if (node.contains("maxLength") && node["maxLength"].is_number_unsigned()) {
        outField.maxLength = node["maxLength"].get<std::size_t>();
    }

    if (outField.kind == FieldKind::Object) {
        if (!node.contains("fields") || !node["fields"].is_array()) {
            error = fmt::format("object field '{}' must declare 'fields'", outField.name);
            return false;
        }
        for (const auto& child : node["fields"]) {
            SchemaField childField;
            if (!ParseFieldNode(child, childField, error)) {
                error = fmt::format("field '{}': {}", outField.name, error);
                return false;
            }
            outField.properties.push_back(std::move(childField));
        }
    }

    if (outField.kind == FieldKind::Array) {
        if (!node.contains("items") || !node["items"].is_object()) {
            error = fmt::format("array field '{}' must declare 'items'", outField.name);
            return false;
        }
        auto element = std::make_unique<SchemaField>();
        if (!ParseFieldNode(node["items"], *element, error)) {
            error = fmt::format("field '{}': {}", outField.name, error);
            return false;
        }
        outField.element = std::move(element);
    }

    return true;
}

bool LoadSchemaFile(const std::filesystem::path& path, ContentSchema& outSchema, std::string& error) {
    nlohmann::json schemaJson;
    if (!SimpleYaml::LoadStructuredFile(path, schemaJson, error)) {
        return false;
    }
    if (!ContentSchemaRegistry::ParseSchema(schemaJson, outSchema, error)) {
        return false;
    }
    outSchema.sourceFile = path.string();
    return true;
}

} // namespace

ContentSchemaRegistry& ContentSchemaRegistry::Instance() {
    static ContentSchemaRegistry registry;
    return registry;
}

void ContentSchemaRegistry::SetAssetRoot(std::filesystem::path assetsRoot) {
    m_assetsRoot = std::move(assetsRoot);
}

void ContentSchemaRegistry::ReloadAll() {
    if (m_assetsRoot.empty()) {
        gm::core::Logger::Warning("[ContentSchemaRegistry] Asset root not set; cannot load schemas");
        return;
    }

    std::unordered_map<std::string, ContentSchema> schemas;
    const auto schemaDir = m_assetsRoot / "content" / "schemas";
    if (std::filesystem::exists(schemaDir)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(schemaDir)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto ext = ToLowerCopy(entry.path().extension().string());
            if (ext != ".json" && ext != ".yaml" && ext != ".yml") {
                continue;
            }
            ContentSchema schema;
            std::string error;
            if (!LoadSchemaFile(entry.path(), schema, error)) {
                gm::core::Logger::Error("[ContentSchemaRegistry] Failed to load schema '{}': {}", entry.path().string(), error);
                continue;
            }
            schemas[schema.type] = std::move(schema);
        }
    } else {
        gm::core::Logger::Warning("[ContentSchemaRegistry] Schema directory '{}' not found", schemaDir.string());
    }

    {
        std::unique_lock lock(m_mutex);
        m_schemas = std::move(schemas);
    }
}

void ContentSchemaRegistry::ReloadSchemaFile(const std::filesystem::path& absolutePath) {
    if (absolutePath.empty()) {
        return;
    }
    ContentSchema schema;
    std::string error;
    if (!LoadSchemaFile(absolutePath, schema, error)) {
        gm::core::Logger::Error("[ContentSchemaRegistry] Failed to reload schema '{}': {}", absolutePath.string(), error);
        return;
    }

    {
        std::unique_lock lock(m_mutex);
        m_schemas[schema.type] = std::move(schema);
    }
}

void ContentSchemaRegistry::RemoveSchemaBySource(const std::filesystem::path& absolutePath) {
    std::unique_lock lock(m_mutex);
    for (auto it = m_schemas.begin(); it != m_schemas.end();) {
        if (std::filesystem::equivalent(it->second.sourceFile, absolutePath)) {
            gm::core::Logger::Info("[ContentSchemaRegistry] Removed schema '{}'", it->first);
            it = m_schemas.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::string> ContentSchemaRegistry::GetRegisteredTypes() const {
    std::shared_lock lock(m_mutex);
    std::vector<std::string> types;
    types.reserve(m_schemas.size());
    for (const auto& [type, _] : m_schemas) {
        types.push_back(type);
    }
    std::sort(types.begin(), types.end());
    return types;
}

std::optional<ContentSchema> ContentSchemaRegistry::GetSchema(const std::string& type) const {
    std::shared_lock lock(m_mutex);
    auto it = m_schemas.find(type);
    if (it == m_schemas.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool ContentSchemaRegistry::ValidateDocument(const ContentSchema& schema,
                                             const nlohmann::json& document,
                                             std::vector<ValidationIssue>& issues) const {
    bool ok = true;
    if (!document.is_object()) {
        issues.push_back({true, schema.type, "Root must be an object"});
        return false;
    }

    for (const auto& field : schema.fields) {
        const std::string path = field.name;
        const auto it = document.find(field.name);
        if (it == document.end()) {
            if (field.required) {
                issues.push_back({true, path, "Required field missing"});
                ok = false;
            }
            continue;
        }
        ok &= ValidateField(field, *it, path, issues);
    }
    return ok;
}

bool ContentSchemaRegistry::ParseSchema(const nlohmann::json& source,
                                        ContentSchema& outSchema,
                                        std::string& error) {
    if (!source.contains("type") || !source["type"].is_string()) {
        error = "schema missing 'type'";
        return false;
    }
    if (!source.contains("fields") || !source["fields"].is_array()) {
        error = "schema missing 'fields' array";
        return false;
    }

    outSchema.type = ToLowerCopy(source["type"].get<std::string>());
    outSchema.displayName = source.value("displayName", outSchema.type);
    outSchema.version = source.value("version", 1);

    outSchema.dataDirectories.clear();
    if (source.contains("dataDirectories") && source["dataDirectories"].is_array()) {
        for (const auto& entry : source["dataDirectories"]) {
            if (entry.is_string()) {
                auto dir = entry.get<std::string>();
                outSchema.dataDirectories.push_back(ToLowerCopy(dir));
            }
        }
    }
    if (outSchema.dataDirectories.empty()) {
        outSchema.dataDirectories.push_back(fmt::format("content/data/{}s", outSchema.type));
    }

    outSchema.fields.clear();
    for (const auto& fieldJson : source["fields"]) {
        SchemaField field;
        if (!ParseFieldNode(fieldJson, field, error)) {
            error = fmt::format("schema '{}': {}", outSchema.type, error);
            return false;
        }
        outSchema.fields.push_back(std::move(field));
    }
    return true;
}

bool ContentSchemaRegistry::ValidateField(const SchemaField& field,
                                          const nlohmann::json& value,
                                          const std::string& path,
                                          std::vector<ValidationIssue>& issues) const {
    bool ok = true;
    switch (field.kind) {
    case FieldKind::String:
        if (!value.is_string()) {
            issues.push_back({true, path, "Expected string"});
            return false;
        }
        if (field.minLength && value.get<std::string>().size() < *field.minLength) {
            issues.push_back({true, path, fmt::format("Minimum length {}", *field.minLength)});
            ok = false;
        }
        if (field.maxLength && value.get<std::string>().size() > *field.maxLength) {
            issues.push_back({true, path, fmt::format("Maximum length {}", *field.maxLength)});
            ok = false;
        }
        if (!field.enumValues.empty()) {
            const auto& str = value.get_ref<const std::string&>();
            if (std::find(field.enumValues.begin(), field.enumValues.end(), str) == field.enumValues.end()) {
                issues.push_back({true, path, "Value not in allowed set"});
                ok = false;
            }
        }
        break;
    case FieldKind::Integer:
        if (!value.is_number_integer()) {
            issues.push_back({true, path, "Expected integer"});
            return false;
        }
        if (field.minimum && value.get<double>() < *field.minimum) {
            issues.push_back({true, path, fmt::format("Minimum {}", *field.minimum)});
            ok = false;
        }
        if (field.maximum && value.get<double>() > *field.maximum) {
            issues.push_back({true, path, fmt::format("Maximum {}", *field.maximum)});
            ok = false;
        }
        break;
    case FieldKind::Number:
        if (!value.is_number()) {
            issues.push_back({true, path, "Expected number"});
            return false;
        }
        if (field.minimum && value.get<double>() < *field.minimum) {
            issues.push_back({true, path, fmt::format("Minimum {}", *field.minimum)});
            ok = false;
        }
        if (field.maximum && value.get<double>() > *field.maximum) {
            issues.push_back({true, path, fmt::format("Maximum {}", *field.maximum)});
            ok = false;
        }
        break;
    case FieldKind::Boolean:
        if (!value.is_boolean()) {
            issues.push_back({true, path, "Expected boolean"});
            return false;
        }
        break;
    case FieldKind::Object:
        if (!value.is_object()) {
            issues.push_back({true, path, "Expected object"});
            return false;
        }
        for (const auto& child : field.properties) {
            const auto it = value.find(child.name);
            std::string childPath = fmt::format("{}.{}", path, child.name);
            if (it == value.end()) {
                if (child.required) {
                    issues.push_back({true, childPath, "Required field missing"});
                    ok = false;
                }
                continue;
            }
            ok &= ValidateField(child, *it, childPath, issues);
        }
        break;
    case FieldKind::Array:
        if (!value.is_array()) {
            issues.push_back({true, path, "Expected array"});
            return false;
        }
        if (field.element) {
            for (std::size_t index = 0; index < value.size(); ++index) {
                const auto& element = value[index];
                std::string elementPath = fmt::format("{}[{}]", path, index);
                ok &= ValidateField(*field.element, element, elementPath, issues);
            }
        }
        break;
    }
    return ok;
}

} // namespace gm::content


