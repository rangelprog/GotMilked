#pragma once

#include <glm/vec3.hpp>
#include <cstdint>
#include <string>

struct WeatherProfile {
    std::string name;
    float spawnMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
    float sizeMultiplier = 1.0f;
    glm::vec3 tint = glm::vec3(1.0f);
    float surfaceWetness = 0.0f;
    float puddleAmount = 0.0f;
    float surfaceDarkening = 0.0f;
    glm::vec3 surfaceTint = glm::vec3(1.0f);
};

struct WeatherState {
    std::string activeProfile = "default";
    glm::vec3 windDirection = glm::vec3(0.2f, 0.0f, 0.8f);
    float windSpeed = 6.0f;
    float surfaceWetness = 0.0f;
    float puddleAmount = 0.0f;
    float surfaceDarkening = 0.0f;
    glm::vec3 surfaceTint = glm::vec3(1.0f);
};

enum class WeatherQuality : std::uint32_t {
    Low,
    Medium,
    High
};

struct WeatherStateEventPayload {
    WeatherState state;
};


