#include "gm/scene/ComponentFactory.hpp"
#include "gm/core/Logger.hpp"

#include <algorithm>

namespace gm::scene {

ComponentFactory& ComponentFactory::Instance() {
    static ComponentFactory instance;
    return instance;
}

bool ComponentFactory::Unregister(const std::string& typeName) {
    auto it = m_creators.find(typeName);
    if (it != m_creators.end()) {
        m_creators.erase(it);
        return true;
    }
    return false;
}

std::shared_ptr<Component> ComponentFactory::Create(const std::string& typeName, GameObject* obj) const {
    if (!obj) {
        core::Logger::Error("[ComponentFactory] Cannot create component '{}': invalid GameObject", 
                    typeName);
        return nullptr;
    }

    auto it = m_creators.find(typeName);
    if (it == m_creators.end()) {
        core::Logger::Error("[ComponentFactory] Component type '{}' is not registered", 
                    typeName);
        return nullptr;
    }

    try {
        auto component = it->second(obj);
        if (!component) {
            core::Logger::Error("[ComponentFactory] Creator function returned null for type '{}'", 
                        typeName);
            return nullptr;
        }
        return component;
    } catch (const std::exception& ex) {
        core::Logger::Error("[ComponentFactory] Exception while creating component '{}': {}", 
                    typeName, ex.what());
        return nullptr;
    } catch (...) {
        core::Logger::Error("[ComponentFactory] Unknown exception while creating component '{}'", 
                    typeName);
        return nullptr;
    }
}

bool ComponentFactory::IsRegistered(const std::string& typeName) const {
    return m_creators.find(typeName) != m_creators.end();
}

std::vector<std::string> ComponentFactory::GetRegisteredTypes() const {
    std::vector<std::string> types;
    types.reserve(m_creators.size());
    for (const auto& pair : m_creators) {
        types.push_back(pair.first);
    }
    std::sort(types.begin(), types.end());
    return types;
}

void ComponentFactory::Clear() {
    m_creators.clear();
}

} // namespace gm::scene

