#include "gm/core/Time.hpp"

namespace gm {
namespace core {

std::chrono::high_resolution_clock::time_point Time::lastUpdate = std::chrono::high_resolution_clock::now();
float Time::deltaTime = 0.0f;
float Time::totalTime = 0.0f;

}} // namespace gm::core