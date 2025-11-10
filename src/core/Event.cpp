#include "gm/core/Event.hpp"

namespace gm {
namespace core {

std::unordered_map<std::string, std::vector<Event::EventCallback>> Event::callbacks;
std::unordered_map<std::string, std::vector<Event::EventCallbackWithData>> Event::callbacksWithData;

}} // namespace gm::core