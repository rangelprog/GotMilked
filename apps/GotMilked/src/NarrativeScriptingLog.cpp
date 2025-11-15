#include "NarrativeScriptingLog.hpp"

#include <algorithm>
#include <cstdio>

#include "gm/core/Logger.hpp"

namespace {

std::string LocationString(const glm::vec3& v) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.2f, %.2f, %.2f", v.x, v.y, v.z);
    return std::string(buffer);
}

} // namespace

NarrativeScriptingLog::NarrativeScriptingLog(const std::shared_ptr<ScriptingHooks>& hooks)
    : m_hooks(hooks) {
    if (!m_hooks) {
        return;
    }
    m_questHook = m_hooks->RegisterQuestHook(
        [this](const ScriptingHooks::QuestEvent& evt) { AppendQuest(evt); });
    m_dialogHook = m_hooks->RegisterDialogueHook(
        [this](const ScriptingHooks::DialogueEvent& evt) { AppendDialogue(evt); });
}

NarrativeScriptingLog::~NarrativeScriptingLog() {
    if (!m_hooks) {
        return;
    }
    if (m_questHook) {
        m_hooks->Unregister(m_questHook);
    }
    if (m_dialogHook) {
        m_hooks->Unregister(m_dialogHook);
    }
}

void NarrativeScriptingLog::Clear() {
    m_entries.clear();
}

void NarrativeScriptingLog::SetMaxEntries(std::size_t maxEntries) {
    m_maxEntries = std::max<std::size_t>(4, maxEntries);
    Trim();
}

void NarrativeScriptingLog::AppendQuest(const ScriptingHooks::QuestEvent& evt) {
    Entry entry;
    entry.type = Entry::Type::Quest;
    entry.identifier = evt.questId;
    entry.subject = evt.triggerObject;
    entry.location = evt.location;
    entry.repeatable = evt.repeatable;
    entry.sceneLoad = evt.triggeredFromSceneLoad;
    entry.timestamp = std::chrono::system_clock::now();
    m_entries.push_back(std::move(entry));
    Trim();

    gm::core::Logger::Info("[Narrative] Quest event '%s' from '%s' at [%s]%s",
        evt.questId.c_str(),
        evt.triggerObject.c_str(),
        LocationString(evt.location).c_str(),
        evt.triggeredFromSceneLoad ? " (scene load)" : "");
}

void NarrativeScriptingLog::AppendDialogue(const ScriptingHooks::DialogueEvent& evt) {
    Entry entry;
    entry.type = Entry::Type::Dialogue;
    entry.identifier = evt.dialogueId;
    entry.subject = evt.speakerObject;
    entry.location = evt.location;
    entry.repeatable = evt.repeatable;
    entry.sceneLoad = evt.triggeredFromSceneLoad;
    entry.autoStart = evt.autoStart;
    entry.timestamp = std::chrono::system_clock::now();
    m_entries.push_back(std::move(entry));
    Trim();

    gm::core::Logger::Info("[Narrative] Dialogue event '%s' (%s) at [%s]%s",
        evt.dialogueId.c_str(),
        evt.speakerObject.empty() ? "unknown" : evt.speakerObject.c_str(),
        LocationString(evt.location).c_str(),
        evt.triggeredFromSceneLoad ? " (scene load)" : "");
}

void NarrativeScriptingLog::Trim() {
    while (m_entries.size() > m_maxEntries) {
        m_entries.pop_front();
    }
}


