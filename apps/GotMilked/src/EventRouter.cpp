#include "EventRouter.hpp"

void EventRouter::Register(const std::string& eventName, gm::core::Event::EventCallback callback) {
    if (!callback) {
        return;
    }
    auto handle = gm::core::Event::Subscribe(eventName, std::move(callback));
    m_subscriptions.emplace_back(std::move(handle));
}

void EventRouter::Clear() {
    m_subscriptions.clear();
}

