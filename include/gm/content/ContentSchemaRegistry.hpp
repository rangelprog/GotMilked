#pragma once

#include "gm/content/ContentTypes.hpp"

#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace gm::content {

class ContentSchemaRegistry {
public:
    using SchemaMap = std::unordered_map<std::string, ContentSchema>;

    static ContentSchemaRegistry& Instance();

    void SetAssetRoot(std::filesystem::path assetsRoot);

    void ReloadAll();
    void ReloadSchemaFile(const std::filesystem::path& absolutePath);
    void RemoveSchemaBySource(const std::filesystem::path& absolutePath);

    [[nodiscard]] std::vector<std::string> GetRegisteredTypes() const;
    [[nodiscard]] std::optional<ContentSchema> GetSchema(const std::string& type) const;

    // Validation utilities
    bool ValidateDocument(const ContentSchema& schema,
                          const nlohmann::json& document,
                          std::vector<ValidationIssue>& issues) const;

    static bool ParseSchema(const nlohmann::json& source, ContentSchema& outSchema, std::string& error);

private:
    ContentSchemaRegistry() = default;

    bool ValidateField(const SchemaField& field,
                       const nlohmann::json& document,
                       const std::string& path,
                       std::vector<ValidationIssue>& issues) const;

    std::filesystem::path m_assetsRoot;
    mutable std::shared_mutex m_mutex;
    SchemaMap m_schemas;
};

} // namespace gm::content


