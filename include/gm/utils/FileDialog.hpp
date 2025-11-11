#pragma once

#include <string>
#include <optional>

namespace gm::utils {

/**
 * @brief Platform-agnostic file dialog interface
 */
class FileDialog {
public:
    /**
     * @brief Show a file save dialog
     * @param filter File filter (e.g., "JSON Files\0*.json\0All Files\0*.*\0")
     * @param defaultExtension Default file extension (e.g., "json")
     * @param initialDir Initial directory path (optional)
     * @param windowHandle Platform-specific window handle (optional, for parent window)
     * @return Selected file path, or empty if cancelled
     */
    static std::optional<std::string> SaveFile(
        const std::string& filter,
        const std::string& defaultExtension,
        const std::string& initialDir = "",
        void* windowHandle = nullptr);

    /**
     * @brief Show a file open dialog
     * @param filter File filter (e.g., "JSON Files\0*.json\0All Files\0*.*\0")
     * @param initialDir Initial directory path (optional)
     * @param windowHandle Platform-specific window handle (optional, for parent window)
     * @return Selected file path, or empty if cancelled
     */
    static std::optional<std::string> OpenFile(
        const std::string& filter,
        const std::string& initialDir = "",
        void* windowHandle = nullptr);
};

} // namespace gm::utils

