#pragma once

#include "gm/assets/AssetCatalog.hpp"
#include "gm/content/ContentSchemaRegistry.hpp"

#include <functional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace gm::content {

struct ContentEvent {
    enum class Type {
        RecordUpdated,
        RecordRemoved,
        SchemaReloaded
    };

    Type type = Type::RecordUpdated;
    std::string contentType;
    std::string identifier;
    std::filesystem::path sourcePath;
    bool valid = true;
};

class ContentDatabase {
public:
    using Listener = std::function<void(const ContentEvent&)>;

    ContentDatabase() = default;
    ~ContentDatabase();

    void Initialize(const std::filesystem::path& assetsRoot);
    void Shutdown();

    void SetNotificationCallback(std::function<void(const std::string&, bool)> callback);

    [[nodiscard]] std::vector<std::string> GetRegisteredTypes() const;
    [[nodiscard]] std::vector<ContentRecord> GetRecordsSnapshot(const std::string& type) const;
    [[nodiscard]] std::vector<ValidationIssue> GetIssuesSnapshot() const;

    void RegisterListener(Listener listener);

private:
    void HandleAssetEvent(const gm::assets::AssetEvent& event);
    void ReloadAllSchemas();
    void ReloadAllContent();
    void ReloadSchemaFile(const std::filesystem::path& absolutePath);
    void ReloadContentFile(const std::filesystem::path& absolutePath,
                           const std::string& relativeLower);
    void RemoveContentFile(const std::string& relativeLower);
    bool LoadDocument(const std::filesystem::path& path,
                      nlohmann::json& document,
                      std::string& error) const;
    std::string DetermineContentType(const std::string& relativeLower) const;
    void ValidateRecord(ContentRecord& record);
    void Notify(const ContentEvent& event);
    static std::string MakeIdentifier(const ContentRecord& record);

    std::filesystem::path m_assetsRoot;
    gm::assets::AssetCatalog::ListenerId m_catalogListener = 0;
    std::function<void(const std::string&, bool)> m_notify;

    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, std::vector<ContentRecord>> m_recordsByType;
    std::unordered_map<std::string, std::string> m_relativePathToType;
    std::vector<Listener> m_listeners;
    std::atomic<std::uint64_t> m_recordVersion{0};
};

} // namespace gm::content


