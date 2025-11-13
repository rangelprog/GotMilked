#include "gm/save/SaveVersion.hpp"

#include <array>
#include <charconv>
#include <sstream>

#include <nlohmann/json.hpp>

#include "gm/core/Logger.hpp"

namespace gm::save {

namespace {

int ParseComponent(std::string_view str, const char* label) {
    int value = 0;
    auto result = std::from_chars(str.data(), str.data() + str.size(), value);
    if (result.ec != std::errc{} || result.ptr != str.data() + str.size()) {
        gm::core::Logger::Warning("[SaveVersion] Failed to parse {} component '{}'", label, str);
        return 0;
    }
    return value;
}

} // namespace

std::string SaveVersion::ToString() const {
    std::ostringstream oss;
    oss << major << '.' << minor << '.' << patch;
    if (!prerelease.empty()) {
        oss << '-' << prerelease;
    }
    return oss.str();
}

bool SaveVersion::IsCompatibleWith(const SaveVersion& runtime) const {
    // Compatibility rule: major must match; newer minor/patch are acceptable if runtime >= save.
    if (major != runtime.major) {
        return false;
    }
    if (minor > runtime.minor) {
        return false;
    }
    if (minor == runtime.minor && patch > runtime.patch) {
        return false;
    }
    return true;
}

SaveVersion ParseSaveVersion(const nlohmann::json& json) {
    if (json.is_object()) {
        SaveVersion version;
        version.major = json.value("major", version.major);
        version.minor = json.value("minor", version.minor);
        version.patch = json.value("patch", version.patch);
        version.prerelease = json.value("prerelease", version.prerelease);
        return version;
    }
    if (json.is_string()) {
        return ParseSaveVersion(std::string_view(json.get_ref<const nlohmann::json::string_t&>()));
    }
    return SaveVersion{};
}

SaveVersion ParseSaveVersion(std::string_view versionString) {
    SaveVersion version;
    if (versionString.empty()) {
        return version;
    }

    std::string_view core = versionString;
    auto dashPos = versionString.find('-');
    if (dashPos != std::string_view::npos) {
        core = versionString.substr(0, dashPos);
        version.prerelease = std::string(versionString.substr(dashPos + 1));
    }

    std::array<std::string_view, 3> components{};
    size_t start = 0;
    size_t index = 0;
    while (start <= core.size() && index < components.size()) {
        size_t dotPos = core.find('.', start);
        if (dotPos == std::string_view::npos) {
            components[index++] = core.substr(start);
            break;
        }
        components[index++] = core.substr(start, dotPos - start);
        start = dotPos + 1;
    }

    if (index >= 1) {
        version.major = ParseComponent(components[0], "major");
    }
    if (index >= 2) {
        version.minor = ParseComponent(components[1], "minor");
    }
    if (index >= 3) {
        version.patch = ParseComponent(components[2], "patch");
    }

    return version;
}

nlohmann::json SaveVersionToJson(const SaveVersion& version) {
    return nlohmann::json{
        {"major", version.major},
        {"minor", version.minor},
        {"patch", version.patch},
        {"prerelease", version.prerelease}
    };
}

} // namespace gm::save


