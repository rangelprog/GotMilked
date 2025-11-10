#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace gm {
namespace core {

class Event {
public:
    using EventCallback = std::function<void()>;
    using EventCallbackWithData = std::function<void(const void*)>;

    static void Subscribe(const std::string& eventName, EventCallback callback) {
        callbacks[eventName].push_back(callback);
    }

    static void SubscribeWithData(const std::string& eventName, EventCallbackWithData callback) {
        callbacksWithData[eventName].push_back(callback);
    }

    static void Trigger(const std::string& eventName) {
        auto it = callbacks.find(eventName);
        if (it != callbacks.end()) {
            for (const auto& callback : it->second) {
                callback();
            }
        }
    }

    static void TriggerWithData(const std::string& eventName, const void* data) {
        auto it = callbacksWithData.find(eventName);
        if (it != callbacksWithData.end()) {
            for (const auto& callback : it->second) {
                callback(data);
            }
        }
    }

private:
    static std::unordered_map<std::string, std::vector<EventCallback>> callbacks;
    static std::unordered_map<std::string, std::vector<EventCallbackWithData>> callbacksWithData;
};

}} // namespace gm::core