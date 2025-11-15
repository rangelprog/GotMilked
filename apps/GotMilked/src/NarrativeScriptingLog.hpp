#pragma once

#include <chrono>
#include <deque>
#include <memory>
#include <string>

#include <glm/vec3.hpp>

#include "ScriptingHooks.hpp"

/**
 * @brief Captures quest/dialogue scripting events for designers and tools.
 *
 * Registers itself with @ref ScriptingHooks and keeps a short rolling history
 * that can be displayed inside the tooling overlay.
 */
class NarrativeScriptingLog {
public:
    struct Entry {
        enum class Type {
            Quest,
            Dialogue
        };

        Type type = Type::Quest;
        std::string identifier;
        std::string subject;      ///< Owning GameObject name (if any)
        glm::vec3 location{0.0f};
        bool repeatable = false;
        bool sceneLoad = false;
        bool autoStart = false;   ///< Only meaningful for dialogue entries
        std::chrono::system_clock::time_point timestamp{};
    };

    explicit NarrativeScriptingLog(const std::shared_ptr<ScriptingHooks>& hooks);
    ~NarrativeScriptingLog();

    NarrativeScriptingLog(const NarrativeScriptingLog&) = delete;
    NarrativeScriptingLog& operator=(const NarrativeScriptingLog&) = delete;

    const std::deque<Entry>& Entries() const { return m_entries; }
    void Clear();
    void SetMaxEntries(std::size_t maxEntries);

private:
    void AppendQuest(const ScriptingHooks::QuestEvent& evt);
    void AppendDialogue(const ScriptingHooks::DialogueEvent& evt);
    void Trim();

    std::shared_ptr<ScriptingHooks> m_hooks;
    ScriptingHooks::HookId m_questHook = 0;
    ScriptingHooks::HookId m_dialogHook = 0;
    std::deque<Entry> m_entries;
    std::size_t m_maxEntries = 64;
};


