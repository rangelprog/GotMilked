#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

/**
 * @brief Lightweight dispatcher for gameplay scripting hooks.
 *
 * Future scripting integrations (Lua, JSON logic, etc.) can subscribe to
 * quest or dialogue events without depending on engine internals.
 */
class ScriptingHooks {
public:
    struct QuestEvent {
        std::string questId;
        std::string triggerObject;
        glm::vec3 location{0.0f};
        bool repeatable = false;
        bool triggeredFromSceneLoad = false;
    };

    struct DialogueEvent {
        std::string dialogueId;
        std::string speakerObject;
        glm::vec3 location{0.0f};
        bool repeatable = false;
        bool autoStart = true;
        bool triggeredFromSceneLoad = false;
    };

    using QuestCallback = std::function<void(const QuestEvent&)>;
    using DialogueCallback = std::function<void(const DialogueEvent&)>;
    using HookId = std::uint64_t;

    HookId RegisterQuestHook(QuestCallback callback);
    HookId RegisterDialogueHook(DialogueCallback callback);
    void Unregister(HookId id);
    void Clear();

    void DispatchQuestEvent(const QuestEvent& event);
    void DispatchDialogueEvent(const DialogueEvent& event);

private:
    struct QuestHookEntry {
        HookId id;
        QuestCallback callback;
    };

    struct DialogueHookEntry {
        HookId id;
        DialogueCallback callback;
    };

    HookId m_nextId = 1;
    std::vector<QuestHookEntry> m_questHooks;
    std::vector<DialogueHookEntry> m_dialogueHooks;
    std::mutex m_mutex;
};


