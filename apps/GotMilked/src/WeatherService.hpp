#pragma once

#include <mutex>
#include <memory>

#include "WeatherTypes.hpp"
#include "gm/scene/TimeOfDayController.hpp"

/**
 * @brief Provides centralized access to the game's weather and time-of-day state.
 *
 * Systems can query this service to retrieve the latest weather conditions,
 * upcoming forecast information, and the currently evaluated sun/moon state.
 */
class WeatherService {
public:
    struct TimeOfDaySnapshot {
        float normalizedTime = 0.0f;            ///< Normalized [0-1) time-of-day.
        float dayLengthSeconds = 0.0f;          ///< Duration of a full day in seconds.
        gm::scene::SunMoonState sunMoonState{}; ///< Latest evaluated sun/moon data.
    };

    struct Snapshot {
        WeatherState weather;
        WeatherForecast forecast;
        TimeOfDaySnapshot timeOfDay;
    };

    struct EnvironmentSnapshot {
        float ambientTemperatureC = 20.0f;
        float precipitationRate = 0.0f; ///< mm/hour approximation
        float surfaceWetness = 0.0f;
    };

    WeatherService();
    ~WeatherService() = default;

    void SetCurrentWeather(const WeatherState& state);
    void SetTimeOfDay(float normalizedTime,
                      float dayLengthSeconds,
                      const gm::scene::SunMoonState& sunState);
    void SetForecast(const WeatherForecast& forecast);
    void SetEnvironment(const EnvironmentSnapshot& environment);

    WeatherState GetCurrentWeather() const;
    WeatherForecast GetForecast() const;
    TimeOfDaySnapshot GetTimeOfDay() const;
    Snapshot GetSnapshot() const;
    EnvironmentSnapshot GetEnvironment() const;

    static void SetGlobalInstance(const std::shared_ptr<WeatherService>& instance);
    static std::shared_ptr<WeatherService> GlobalInstance();

private:
    mutable std::mutex m_mutex;
    WeatherState m_currentWeather{};
    WeatherForecast m_forecast{};
    TimeOfDaySnapshot m_timeOfDay{};
    EnvironmentSnapshot m_environment{};

    static std::mutex s_instanceMutex;
    static std::weak_ptr<WeatherService> s_instance;
};


