#pragma once

#include <string>
#include <vector>
#include <utility>

#include "gm/core/Event.hpp"

class EventRouter {
public:
    EventRouter() = default;
    ~EventRouter() = default;

    void Register(const std::string& eventName, gm::core::Event::EventCallback callback);
    void Clear();

private:
    std::vector<gm::core::Event::ScopedSubscription> m_subscriptions;
};

