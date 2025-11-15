#include "ScriptingHooks.hpp"

#include <algorithm>

ScriptingHooks::HookId ScriptingHooks::RegisterQuestHook(QuestCallback callback) {
    if (!callback) {
        return 0;
    }
    std::scoped_lock lock(m_mutex);
    const HookId id = m_nextId++;
    m_questHooks.push_back(QuestHookEntry{id, std::move(callback)});
    return id;
}

ScriptingHooks::HookId ScriptingHooks::RegisterDialogueHook(DialogueCallback callback) {
    if (!callback) {
        return 0;
    }
    std::scoped_lock lock(m_mutex);
    const HookId id = m_nextId++;
    m_dialogueHooks.push_back(DialogueHookEntry{id, std::move(callback)});
    return id;
}

void ScriptingHooks::Unregister(HookId id) {
    if (id == 0) {
        return;
    }
    std::scoped_lock lock(m_mutex);
    auto questIt = std::remove_if(m_questHooks.begin(), m_questHooks.end(),
        [id](const QuestHookEntry& entry) { return entry.id == id; });
    m_questHooks.erase(questIt, m_questHooks.end());

    auto dialogIt = std::remove_if(m_dialogueHooks.begin(), m_dialogueHooks.end(),
        [id](const DialogueHookEntry& entry) { return entry.id == id; });
    m_dialogueHooks.erase(dialogIt, m_dialogueHooks.end());
}

void ScriptingHooks::Clear() {
    std::scoped_lock lock(m_mutex);
    m_questHooks.clear();
    m_dialogueHooks.clear();
}

void ScriptingHooks::DispatchQuestEvent(const QuestEvent& event) {
    std::vector<QuestHookEntry> hooksCopy;
    {
        std::scoped_lock lock(m_mutex);
        hooksCopy = m_questHooks;
    }
    for (const auto& hook : hooksCopy) {
        if (hook.callback) {
            hook.callback(event);
        }
    }
}

void ScriptingHooks::DispatchDialogueEvent(const DialogueEvent& event) {
    std::vector<DialogueHookEntry> hooksCopy;
    {
        std::scoped_lock lock(m_mutex);
        hooksCopy = m_dialogueHooks;
    }
    for (const auto& hook : hooksCopy) {
        if (hook.callback) {
            hook.callback(event);
        }
    }
}


