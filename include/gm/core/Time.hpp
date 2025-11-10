#pragma once
#include <chrono>

namespace gm {
namespace core {

class Time {
public:
    static void Update() {
        auto now = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(now - lastUpdate).count();
        lastUpdate = now;
        totalTime += deltaTime;
    }

    static float DeltaTime() { return deltaTime; }
    static float TotalTime() { return totalTime; }

private:
    static std::chrono::high_resolution_clock::time_point lastUpdate;
    static float deltaTime;
    static float totalTime;
};

}} // namespace gm::core