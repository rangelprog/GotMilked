#include "gm/content/ContentDatabase.hpp"

#include "gm/content/SimpleYaml.hpp"
#include "gm/core/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace gm::content {

namespace {

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string DeriveIdentifier(const nlohmann::json& document,
                             const std::filesystem::path& fallbackPath) {
    if (document.contains("id") && document["id"].is_string()) {
        return document["id"].get<std::string>();
    }
    if (document.contains("guid") && document["guid"].is_string()) {
        return document["guid"].get<std::string>();
    }
    return fallbackPath.stem().string();
}

std::string DeriveDisplayName(const nlohmann::json& document,
                              const std::string& identifier) {
    if (document.contains("displayName") && document["displayName"].is_string()) {
        return document["displayName"].get<std::string>();
    }
    if (document.contains("title") && document["title"].is_string()) {
        return document["title"].get<std::string>();
    }
    return identifier;
}

} // namespace

ContentDatabase::~ContentDatabase() {
    Shutdown();
}

void ContentDatabase::Initialize(const std::filesystem::path& assetsRoot) {
    Shutdown();

    std::error_code ec;
    m_assetsRoot = std::filesystem::weakly_canonical(assetsRoot, ec);
    if (ec) {
        m_assetsRoot = std::filesystem::absolute(assetsRoot);
    }
    ContentSchemaRegistry::Instance().SetAssetRoot(m_assetsRoot);
    ContentSchemaRegistry::Instance().ReloadAll();
    ReloadAllContent();

    auto& catalog = gm::assets::AssetCatalog::Instance();
    m_catalogListener = catalog.RegisterListener([this](const gm::assets::AssetEvent& event) {
        HandleAssetEvent(event);
    });
}

void ContentDatabase::Shutdown() {
    auto& catalog = gm::assets::AssetCatalog::Instance();
    if (m_catalogListener != 0) {
        catalog.UnregisterListener(m_catalogListener);
        m_catalogListener = 0;
    }

    {
        std::unique_lock lock(m_mutex);
        m_recordsByType.clear();
        m_relativePathToType.clear();
    }
}

void ContentDatabase::SetNotificationCallback(std::function<void(const std::string&, bool)> callback) {
    m_notify = std::move(callback);
}

std::vector<std::string> ContentDatabase::GetRegisteredTypes() const {
    std::shared_lock lock(m_mutex);
    std::vector<std::string> types;
    types.reserve(m_recordsByType.size());
    for (const auto& [type, _] : m_recordsByType) {
        types.push_back(type);
    }
    std::sort(types.begin(), types.end());
    return types;
}

std::vector<ContentRecord> ContentDatabase::GetRecordsSnapshot(const std::string& type) const {
    std::shared_lock lock(m_mutex);
    if (auto it = m_recordsByType.find(type); it != m_recordsByType.end()) {
        return it->second;
    }
    return {};
}

std::vector<ValidationIssue> ContentDatabase::GetIssuesSnapshot() const {
    std::vector<ValidationIssue> issues;
    std::shared_lock lock(m_mutex);
    for (const auto& [_, records] : m_recordsByType) {
        for (const auto& record : records) {
            issues.insert(issues.end(), record.issues.begin(), record.issues.end());
        }
    }
    return issues;
}

void ContentDatabase::RegisterListener(Listener listener) {
    if (!listener) {
        return;
    }
    std::unique_lock lock(m_mutex);
    m_listeners.push_back(std::move(listener));
}

void ContentDatabase::HandleAssetEvent(const gm::assets::AssetEvent& event) {
    auto relativeLower = ToLowerCopy(event.descriptor.relativePath);
    if (relativeLower.rfind("content/schemas/", 0) == 0) {
        if (event.type == gm::assets::AssetEventType::Removed) {
            ContentSchemaRegistry::Instance().RemoveSchemaBySource(event.descriptor.absolutePath);
        } else {
            ContentSchemaRegistry::Instance().ReloadSchemaFile(event.descriptor.absolutePath);
        }
        ReloadAllContent();
        ContentEvent contentEvent;
        contentEvent.type = ContentEvent::Type::SchemaReloaded;
        contentEvent.contentType = "<schemas>";
        contentEvent.sourcePath = event.descriptor.absolutePath;
        Notify(contentEvent);
        return;
    }

    if (relativeLower.rfind("content/data/", 0) != 0) {
        return;
    }

    if (event.type == gm::assets::AssetEventType::Removed) {
        RemoveContentFile(relativeLower);
    } else {
        ReloadContentFile(event.descriptor.absolutePath, relativeLower);
    }
}

void ContentDatabase::ReloadAllSchemas() {
    ContentSchemaRegistry::Instance().ReloadAll();
}

void ContentDatabase::ReloadAllContent() {
    std::unordered_map<std::string, std::vector<ContentRecord>> records;
    std::unordered_map<std::string, std::string> pathToType;

    auto registryTypes = ContentSchemaRegistry::Instance().GetRegisteredTypes();
    for (const auto& type : registryTypes) {
        auto schemaOpt = ContentSchemaRegistry::Instance().GetSchema(type);
        if (!schemaOpt) {
            continue;
        }
        const auto& schema = *schemaOpt;
        auto& list = records[type];

        for (const auto& dir : schema.dataDirectories) {
            auto absoluteDir = m_assetsRoot / dir;
            if (!std::filesystem::exists(absoluteDir)) {
                continue;
            }
            for (const auto& entry : std::filesystem::recursive_directory_iterator(absoluteDir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                auto ext = ToLowerCopy(entry.path().extension().string());
                if (ext != ".json" && ext != ".yaml" && ext != ".yml") {
                    continue;
                }
                std::error_code ec;
                auto relative = std::filesystem::relative(entry.path(), m_assetsRoot, ec);
                std::string relativeLower = ec ? ToLowerCopy(entry.path().filename().string())
                                               : ToLowerCopy(relative.generic_string());
                ContentRecord record;
                record.type = type;
                record.relativePath = relativeLower;
                record.sourcePath = entry.path();
                record.lastWriteTime = std::filesystem::last_write_time(entry.path(), ec);
                record.version = m_recordVersion.fetch_add(1, std::memory_order_relaxed) + 1;

                std::string error;
                nlohmann::json document;
                if (!LoadDocument(entry.path(), document, error)) {
                    record.valid = false;
                    record.issues.push_back({true, type, error});
                } else {
                    record.document = document;
                    record.identifier = DeriveIdentifier(document, entry.path());
                    record.displayName = DeriveDisplayName(document, record.identifier);
                    record.guid = document.value("guid", record.identifier);
                    ValidateRecord(record);
                }

                if (record.identifier.empty()) {
                    record.identifier = entry.path().stem().string();
                }
                if (record.displayName.empty()) {
                    record.displayName = record.identifier;
                }

                list.push_back(record);
                pathToType[relativeLower] = type;
            }
        }
    }

    {
        std::unique_lock lock(m_mutex);
        m_recordsByType = std::move(records);
        m_relativePathToType = std::move(pathToType);
    }
}

void ContentDatabase::ReloadSchemaFile(const std::filesystem::path& absolutePath) {
    (void)absolutePath;
    ReloadAllSchemas();
    ReloadAllContent();
}

void ContentDatabase::ReloadContentFile(const std::filesystem::path& absolutePath,
                                        const std::string& relativeLower) {
    const auto type = DetermineContentType(relativeLower);
    if (type.empty()) {
        gm::core::Logger::Debug("[ContentDatabase] No schema for content '{}'", relativeLower);
        return;
    }

    ContentRecord record;
    record.type = type;
    record.relativePath = relativeLower;
    record.sourcePath = absolutePath;
    std::error_code ec;
    record.lastWriteTime = std::filesystem::last_write_time(absolutePath, ec);
    record.version = m_recordVersion.fetch_add(1, std::memory_order_relaxed) + 1;

    std::string error;
    nlohmann::json document;
    if (!LoadDocument(absolutePath, document, error)) {
        record.valid = false;
        record.issues.push_back({true, type, error});
    } else {
        record.document = document;
        record.identifier = DeriveIdentifier(document, absolutePath);
        record.displayName = DeriveDisplayName(document, record.identifier);
        record.guid = document.value("guid", record.identifier);
        ValidateRecord(record);
    }

    if (record.identifier.empty()) {
        record.identifier = absolutePath.stem().string();
    }
    if (record.displayName.empty()) {
        record.displayName = record.identifier;
    }

    {
        std::unique_lock lock(m_mutex);
        auto& list = m_recordsByType[type];
        auto it = std::find_if(list.begin(), list.end(), [&](const ContentRecord& existing) {
            return existing.relativePath == relativeLower;
        });
        if (it != list.end()) {
            *it = record;
        } else {
            list.push_back(record);
        }
        m_relativePathToType[relativeLower] = type;
    }

    ContentEvent event;
    event.type = ContentEvent::Type::RecordUpdated;
    event.contentType = type;
    event.identifier = record.identifier;
    event.sourcePath = record.sourcePath;
    event.valid = record.valid;
    Notify(event);

    if (!record.valid && m_notify) {
        m_notify(fmt::format("Content '{}': {}", record.identifier, record.issues.front().message), true);
    } else if (record.valid && m_notify) {
        m_notify(fmt::format("Content '{}': reloaded", record.identifier), false);
    }
}

void ContentDatabase::RemoveContentFile(const std::string& relativeLower) {
    std::string type;
    {
        std::shared_lock lock(m_mutex);
        auto mapIt = m_relativePathToType.find(relativeLower);
        if (mapIt == m_relativePathToType.end()) {
            return;
        }
        type = mapIt->second;
    }

    bool removed = false;
    std::string identifier;
    {
        std::unique_lock lock(m_mutex);
        auto& list = m_recordsByType[type];
        auto it = std::remove_if(list.begin(), list.end(), [&](const ContentRecord& record) {
            if (record.relativePath == relativeLower) {
                identifier = record.identifier;
                return true;
            }
            return false;
        });
        if (it != list.end()) {
            list.erase(it, list.end());
            removed = true;
            m_relativePathToType.erase(relativeLower);
        }
    }

    if (removed) {
        ContentEvent event;
        event.type = ContentEvent::Type::RecordRemoved;
        event.contentType = type;
        event.identifier = identifier;
        Notify(event);
        if (m_notify) {
            m_notify(fmt::format("Content '{}' removed", identifier.empty() ? relativeLower : identifier), false);
        }
    }
}

bool ContentDatabase::LoadDocument(const std::filesystem::path& path,
                                   nlohmann::json& document,
                                   std::string& error) const {
    return SimpleYaml::LoadStructuredFile(path, document, error);
}

std::string ContentDatabase::DetermineContentType(const std::string& relativeLower) const {
    auto schemaTypes = ContentSchemaRegistry::Instance().GetRegisteredTypes();
    for (const auto& type : schemaTypes) {
        auto schemaOpt = ContentSchemaRegistry::Instance().GetSchema(type);
        if (!schemaOpt) {
            continue;
        }
        const auto& schema = *schemaOpt;
        for (const auto& dir : schema.dataDirectories) {
            if (relativeLower.rfind(dir, 0) == 0) {
                return type;
            }
        }
    }
    return {};
}

void ContentDatabase::ValidateRecord(ContentRecord& record) {
    auto schemaOpt = ContentSchemaRegistry::Instance().GetSchema(record.type);
    if (!schemaOpt) {
        record.valid = false;
        record.issues.push_back({true, record.type, "Schema not found"});
        return;
    }

    record.issues.clear();
    record.valid = ContentSchemaRegistry::Instance().ValidateDocument(*schemaOpt, record.document, record.issues);
}

void ContentDatabase::Notify(const ContentEvent& event) {
    std::vector<Listener> listenersCopy;
    {
        std::shared_lock lock(m_mutex);
        listenersCopy = m_listeners;
    }
    for (auto& listener : listenersCopy) {
        if (listener) {
            listener(event);
        }
    }
}

} // namespace gm::content


