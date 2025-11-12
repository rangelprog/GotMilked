#include "gm/scene/Component.hpp"

#include <mutex>
#include <unordered_map>

namespace gm {

namespace {
std::unordered_map<std::type_index, std::string> g_componentTypeNames;
std::mutex g_componentTypeNamesMutex;
} // namespace

const std::string& Component::TypeName(const std::type_index& typeId) {
    {
        std::lock_guard<std::mutex> lock(g_componentTypeNamesMutex);
        auto it = g_componentTypeNames.find(typeId);
        if (it != g_componentTypeNames.end()) {
            return it->second;
        }
    }

    std::string computedName = typeId.name();

    std::lock_guard<std::mutex> lock(g_componentTypeNamesMutex);
    auto [it, inserted] = g_componentTypeNames.emplace(typeId, std::move(computedName));
    if (!inserted) {
        return it->second;
    }
    return it->second;
}

} // namespace gm


