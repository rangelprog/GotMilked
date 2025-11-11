#include "gm/utils/FileDialog.hpp"

#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#else
// For non-Windows platforms, we'll use a simple fallback
// In the future, this could use native dialogs (e.g., GTK on Linux, Cocoa on macOS)
#endif

namespace gm::utils {

std::optional<std::string> FileDialog::SaveFile(
    const std::string& filter,
    const std::string& defaultExtension,
    const std::string& initialDir,
    void* windowHandle)
{
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(windowHandle);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    
    if (!initialDir.empty() && std::filesystem::exists(initialDir)) {
        ofn.lpstrInitialDir = initialDir.c_str();
    }
    
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = defaultExtension.c_str();
    
    if (GetSaveFileNameA(&ofn)) {
        return std::string(szFile);
    }
    
    return std::nullopt;
#else
    // Fallback: return empty (could implement native dialogs here)
    (void)filter;
    (void)defaultExtension;
    (void)initialDir;
    (void)windowHandle;
    return std::nullopt;
#endif
}

std::optional<std::string> FileDialog::OpenFile(
    const std::string& filter,
    const std::string& initialDir,
    void* windowHandle)
{
#ifdef _WIN32
    OPENFILENAMEA ofn;
    char szFile[260] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(windowHandle);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    
    if (!initialDir.empty() && std::filesystem::exists(initialDir)) {
        ofn.lpstrInitialDir = initialDir.c_str();
    }
    
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(szFile);
    }
    
    return std::nullopt;
#else
    // Fallback: return empty (could implement native dialogs here)
    (void)filter;
    (void)initialDir;
    (void)windowHandle;
    return std::nullopt;
#endif
}

} // namespace gm::utils

