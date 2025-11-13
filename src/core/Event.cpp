#include "gm/core/Event.hpp"

#include <algorithm>
#include <utility>

namespace gm {
namespace core {

std::unordered_map<std::string, std::vector<Event::CallbackEntry>> Event::callbacks;
std::unordered_map<std::string, std::vector<Event::CallbackWithDataEntry>> Event::callbacksWithData;
std::atomic<Event::SubscriptionId> Event::nextId{1};

Event::SubscriptionHandle::SubscriptionHandle(std::string eventName, SubscriptionId id, bool withData)
    : m_eventName(std::move(eventName))
    , m_id(id)
    , m_withData(withData) {}

Event::SubscriptionHandle::SubscriptionHandle(SubscriptionHandle&& other) noexcept {
    *this = std::move(other);
}

Event::SubscriptionHandle& Event::SubscriptionHandle::operator=(SubscriptionHandle&& other) noexcept {
    if (this != &other) {
        Reset();
        m_eventName = std::move(other.m_eventName);
        m_id = other.m_id;
        m_withData = other.m_withData;
        m_autoUnsubscribe = other.m_autoUnsubscribe;

        other.m_id = 0;
        other.m_eventName.clear();
        other.m_withData = false;
        other.m_autoUnsubscribe = false;
    }
    return *this;
}

Event::SubscriptionHandle::~SubscriptionHandle() {
    if (m_autoUnsubscribe) {
        Reset();
    }
}

void Event::SubscriptionHandle::Reset() {
    if (m_id == 0) {
        return;
    }
    Event::Unsubscribe(m_eventName, m_id, m_withData);
    m_id = 0;
    m_eventName.clear();
    m_withData = false;
    m_autoUnsubscribe = false;
}

Event::ScopedSubscription::ScopedSubscription(SubscriptionHandle handle)
    : m_handle(std::move(handle)) {
    m_handle.SetAutoUnsubscribe(true);
}

Event::ScopedSubscription::~ScopedSubscription() {
    Reset();
}

void Event::ScopedSubscription::Reset() {
    m_handle.Reset();
}

Event::SubscriptionHandle Event::Subscribe(const std::string& eventName, EventCallback callback) {
    if (!callback) {
        return {};
    }
    auto id = GenerateSubscriptionId();
    callbacks[eventName].push_back(CallbackEntry{ id, std::move(callback), true });
    return SubscriptionHandle(eventName, id, /*withData*/ false);
}

Event::SubscriptionHandle Event::SubscribeWithData(const std::string& eventName, EventCallbackWithData callback) {
    if (!callback) {
        return {};
    }
    auto id = GenerateSubscriptionId();
    callbacksWithData[eventName].push_back(CallbackWithDataEntry{ id, std::move(callback), true });
    return SubscriptionHandle(eventName, id, /*withData*/ true);
}

void Event::Trigger(const std::string& eventName) {
    auto it = callbacks.find(eventName);
    if (it == callbacks.end()) {
        return;
    }

    auto& list = it->second;
    for (auto& entry : list) {
        if (entry.active && entry.callback) {
            entry.callback();
        }
    }

    CleanupInactiveCallbacks(list);
}

void Event::TriggerWithData(const std::string& eventName, const void* data) {
    auto it = callbacksWithData.find(eventName);
    if (it == callbacksWithData.end()) {
        return;
    }

    auto& list = it->second;
    for (auto& entry : list) {
        if (entry.active && entry.callback) {
            entry.callback(data);
        }
    }

    CleanupInactiveCallbacks(list);
}

void Event::Unsubscribe(SubscriptionHandle& handle) {
    if (!handle.IsValid()) {
        return;
    }
    Unsubscribe(handle.m_eventName, handle.m_id, handle.m_withData);
    handle.m_id = 0;
    handle.m_eventName.clear();
    handle.m_withData = false;
    handle.m_autoUnsubscribe = false;
}

void Event::Unsubscribe(const std::string& eventName, SubscriptionId id, bool withData) {
    if (id == 0) {
        return;
    }

    if (withData) {
        auto it = callbacksWithData.find(eventName);
        if (it == callbacksWithData.end()) {
            return;
        }

        auto& list = it->second;
        for (auto& entry : list) {
            if (entry.active && entry.id == id) {
                entry.active = false;
                break;
            }
        }
        CleanupInactiveCallbacks(list);
        return;
    }

    auto it = callbacks.find(eventName);
    if (it == callbacks.end()) {
        return;
    }

    auto& list = it->second;
    for (auto& entry : list) {
        if (entry.active && entry.id == id) {
            entry.active = false;
            break;
        }
    }
    CleanupInactiveCallbacks(list);
}

Event::SubscriptionId Event::GenerateSubscriptionId() {
    SubscriptionId id = nextId.fetch_add(1, std::memory_order_relaxed);
    if (id == 0) {
        id = nextId.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

void Event::CleanupInactiveCallbacks(std::vector<CallbackEntry>& list) {
    list.erase(std::remove_if(list.begin(), list.end(),
                              [](const CallbackEntry& entry) { return !entry.active || !entry.callback; }),
               list.end());
}

void Event::CleanupInactiveCallbacks(std::vector<CallbackWithDataEntry>& list) {
    list.erase(std::remove_if(list.begin(), list.end(),
                              [](const CallbackWithDataEntry& entry) { return !entry.active || !entry.callback; }),
               list.end());
}

}} // namespace gm::core