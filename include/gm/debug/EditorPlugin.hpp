#pragma once

#if GM_DEBUG_TOOLS

#include <functional>
#include <memory>
#include <string>

namespace gm {
class Scene;
}

class GameResources;

namespace gm::debug {

struct ShortcutDesc {
    std::string id;
    std::string key;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

class EditorPluginHost {
public:
    virtual ~EditorPluginHost() = default;

    virtual GameResources* GetGameResources() const = 0;
    virtual std::shared_ptr<gm::Scene> GetActiveScene() const = 0;

    virtual void RegisterDockWindow(const std::string& id,
                                    const std::string& title,
                                    const std::function<void()>& renderFn,
                                    bool* visibilityFlag = nullptr) = 0;

    virtual void RegisterShortcut(const ShortcutDesc& desc,
                                  const std::function<void()>& handler) = 0;

    virtual void PushUndoableAction(const std::string& description,
                                    const std::function<void()>& redo,
                                    const std::function<void()>& undo) = 0;
};

class EditorPlugin {
public:
    virtual ~EditorPlugin() = default;

    virtual const char* Name() const = 0;
    virtual void Initialize(EditorPluginHost& host) = 0;
    virtual void Render(EditorPluginHost& /*host*/) = 0;
    virtual void Shutdown(EditorPluginHost& /*host*/) {}
};

using CreateEditorPluginFn = EditorPlugin* (*)();
using DestroyEditorPluginFn = void (*)(EditorPlugin*);

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


