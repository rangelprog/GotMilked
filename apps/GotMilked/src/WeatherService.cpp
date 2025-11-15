#include "WeatherService.hpp"

std::mutex WeatherService::s_instanceMutex;
std::weak_ptr<WeatherService> WeatherService::s_instance;

WeatherService::WeatherService() = default;

void WeatherService::SetCurrentWeather(const WeatherState& state) {
    std::scoped_lock lock(m_mutex);
    m_currentWeather = state;
}

void WeatherService::SetTimeOfDay(float normalizedTime,
                                  float dayLengthSeconds,
                                  const gm::scene::SunMoonState& sunState) {
    std::scoped_lock lock(m_mutex);
    m_timeOfDay.normalizedTime = normalizedTime;
    m_timeOfDay.dayLengthSeconds = dayLengthSeconds;
    m_timeOfDay.sunMoonState = sunState;
}

void WeatherService::SetForecast(const WeatherForecast& forecast) {
    std::scoped_lock lock(m_mutex);
    m_forecast = forecast;
}

void WeatherService::SetEnvironment(const EnvironmentSnapshot& environment) {
    std::scoped_lock lock(m_mutex);
    m_environment = environment;
}

WeatherState WeatherService::GetCurrentWeather() const {
    std::scoped_lock lock(m_mutex);
    return m_currentWeather;
}

WeatherForecast WeatherService::GetForecast() const {
    std::scoped_lock lock(m_mutex);
    return m_forecast;
}

WeatherService::TimeOfDaySnapshot WeatherService::GetTimeOfDay() const {
    std::scoped_lock lock(m_mutex);
    return m_timeOfDay;
}

WeatherService::Snapshot WeatherService::GetSnapshot() const {
    std::scoped_lock lock(m_mutex);
    Snapshot snapshot{};
    snapshot.weather = m_currentWeather;
    snapshot.forecast = m_forecast;
    snapshot.timeOfDay = m_timeOfDay;
    return snapshot;
}

WeatherService::EnvironmentSnapshot WeatherService::GetEnvironment() const {
    std::scoped_lock lock(m_mutex);
    return m_environment;
}

void WeatherService::SetGlobalInstance(const std::shared_ptr<WeatherService>& instance) {
    std::scoped_lock lock(s_instanceMutex);
    s_instance = instance;
}

std::shared_ptr<WeatherService> WeatherService::GlobalInstance() {
    std::scoped_lock lock(s_instanceMutex);
    return s_instance.lock();
}


