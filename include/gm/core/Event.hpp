#pragma once
#include <atomic>
#include <cstdint>
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

    using SubscriptionId = std::uint64_t;

    class SubscriptionHandle {
    public:
        SubscriptionHandle() = default;
        SubscriptionHandle(const SubscriptionHandle&) = delete;
        SubscriptionHandle& operator=(const SubscriptionHandle&) = delete;
        SubscriptionHandle(SubscriptionHandle&& other) noexcept;
        SubscriptionHandle& operator=(SubscriptionHandle&& other) noexcept;
        ~SubscriptionHandle();

        bool IsValid() const { return m_id != 0; }
        explicit operator bool() const { return IsValid(); }
        SubscriptionId Id() const { return m_id; }
        const std::string& EventName() const { return m_eventName; }
        void Reset();

    private:
        friend class Event;
        friend class ScopedSubscription;
        SubscriptionHandle(std::string eventName, SubscriptionId id, bool withData);
        void SetAutoUnsubscribe(bool enabled) { m_autoUnsubscribe = enabled; }

        std::string m_eventName;
        SubscriptionId m_id = 0;
        bool m_withData = false;
        bool m_autoUnsubscribe = false;
    };

    class ScopedSubscription {
    public:
        ScopedSubscription() = default;
        explicit ScopedSubscription(SubscriptionHandle handle);
        ScopedSubscription(const ScopedSubscription&) = delete;
        ScopedSubscription& operator=(const ScopedSubscription&) = delete;
        ScopedSubscription(ScopedSubscription&& other) noexcept = default;
        ScopedSubscription& operator=(ScopedSubscription&& other) noexcept = default;
        ~ScopedSubscription();

        bool IsValid() const { return m_handle.IsValid(); }
        explicit operator bool() const { return IsValid(); }
        void Reset();

    private:
        SubscriptionHandle m_handle;
    };

    static SubscriptionHandle Subscribe(const std::string& eventName, EventCallback callback);

    static SubscriptionHandle SubscribeWithData(const std::string& eventName, EventCallbackWithData callback);

    static void Trigger(const std::string& eventName);

    static void TriggerWithData(const std::string& eventName, const void* data);

    static void Unsubscribe(SubscriptionHandle& handle);
    static void Unsubscribe(const std::string& eventName, SubscriptionId id, bool withData);

private:
    struct CallbackEntry {
        SubscriptionId id = 0;
        EventCallback callback;
        bool active = true;
    };

    struct CallbackWithDataEntry {
        SubscriptionId id = 0;
        EventCallbackWithData callback;
        bool active = true;
    };

    static SubscriptionId GenerateSubscriptionId();
    static void CleanupInactiveCallbacks(std::vector<CallbackEntry>& list);
    static void CleanupInactiveCallbacks(std::vector<CallbackWithDataEntry>& list);

    static std::unordered_map<std::string, std::vector<CallbackEntry>> callbacks;
    static std::unordered_map<std::string, std::vector<CallbackWithDataEntry>> callbacksWithData;
    static std::atomic<SubscriptionId> nextId;
};

}} // namespace gm::core