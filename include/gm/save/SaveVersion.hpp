#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json_fwd.hpp>

namespace gm::save {

/**
 * @brief Semantic version identifier for save-game files.
 *
 * Save versions are embedded in every save file so that loaders can detect
 * compatibility issues and perform migrations when schemas change.
 */
struct SaveVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease;

    constexpr SaveVersion() = default;
    constexpr SaveVersion(int maj, int min, int pat, std::string pre = {})
        : major(maj), minor(min), patch(pat), prerelease(std::move(pre)) {}

    [[nodiscard]] std::string ToString() const;

    [[nodiscard]] bool operator==(const SaveVersion&) const = default;
    [[nodiscard]] bool operator!=(const SaveVersion&) const = default;

    [[nodiscard]] bool IsCompatibleWith(const SaveVersion& runtime) const;

    static constexpr SaveVersion Current() {
        return SaveVersion(1, 1, 0);
    }
};

SaveVersion ParseSaveVersion(const nlohmann::json& json);
SaveVersion ParseSaveVersion(std::string_view versionString);
nlohmann::json SaveVersionToJson(const SaveVersion& version);

} // namespace gm::save


