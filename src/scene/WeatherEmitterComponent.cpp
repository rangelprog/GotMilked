#include "gm/scene/WeatherEmitterComponent.hpp"

#include "gm/scene/GameObject.hpp"
#include "gm/scene/ComponentRegistration.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

namespace gm {
namespace {

const char* ToString(WeatherEmitterComponent::ParticleType type) {
    switch (type) {
    case WeatherEmitterComponent::ParticleType::Rain: return "rain";
    case WeatherEmitterComponent::ParticleType::Snow: return "snow";
    case WeatherEmitterComponent::ParticleType::Dust: return "dust";
    default: return "rain";
    }
}

WeatherEmitterComponent::ParticleType ParseType(const std::string& value) {
    if (value == "snow") return WeatherEmitterComponent::ParticleType::Snow;
    if (value == "dust") return WeatherEmitterComponent::ParticleType::Dust;
    return WeatherEmitterComponent::ParticleType::Rain;
}

const char* ToString(WeatherEmitterComponent::VolumeShape shape) {
    switch (shape) {
    case WeatherEmitterComponent::VolumeShape::Cylinder: return "cylinder";
    case WeatherEmitterComponent::VolumeShape::Box:
    default: return "box";
    }
}

WeatherEmitterComponent::VolumeShape ParseShape(const std::string& value) {
    if (value == "cylinder") return WeatherEmitterComponent::VolumeShape::Cylinder;
    return WeatherEmitterComponent::VolumeShape::Box;
}

nlohmann::json SerializeEmitter(Component* component) {
    auto* emitter = static_cast<WeatherEmitterComponent*>(component);
    nlohmann::json data;
    data["type"] = ToString(emitter->GetType());
    data["shape"] = ToString(emitter->GetVolumeShape());
    data["volumeExtents"] = { emitter->GetVolumeExtents().x, emitter->GetVolumeExtents().y, emitter->GetVolumeExtents().z };
    data["direction"] = { emitter->GetDirection().x, emitter->GetDirection().y, emitter->GetDirection().z };
    data["spawnRate"] = emitter->GetSpawnRate();
    data["lifetime"] = emitter->GetParticleLifetime();
    data["speed"] = emitter->GetParticleSpeed();
    data["size"] = emitter->GetParticleSize();
    data["alignToWind"] = emitter->GetAlignToWind();
    data["maxParticles"] = {
        {"high", emitter->GetMaxParticlesHigh()},
        {"medium", emitter->GetMaxParticlesMedium()},
        {"low", emitter->GetMaxParticlesLow()}
    };
    data["profile"] = emitter->GetProfileTag();
    data["baseColor"] = { emitter->GetBaseColor().x, emitter->GetBaseColor().y, emitter->GetBaseColor().z };
    return data;
}

Component* DeserializeEmitter(GameObject* owner, const nlohmann::json& data) {
    auto emitter = owner->AddComponent<WeatherEmitterComponent>();
    emitter->SetType(ParseType(data.value("type", "rain")));
    emitter->SetVolumeShape(ParseShape(data.value("shape", "box")));

    if (auto ext = data.find("volumeExtents"); ext != data.end() && ext->is_array() && ext->size() == 3) {
        emitter->SetVolumeExtents(glm::vec3((*ext)[0].get<float>(), (*ext)[1].get<float>(), (*ext)[2].get<float>()));
    }
    if (auto dir = data.find("direction"); dir != data.end() && dir->is_array() && dir->size() == 3) {
        emitter->SetDirection(glm::vec3((*dir)[0].get<float>(), (*dir)[1].get<float>(), (*dir)[2].get<float>()));
    }
    if (auto color = data.find("baseColor"); color != data.end() && color->is_array() && color->size() == 3) {
        emitter->SetBaseColor(glm::vec3((*color)[0].get<float>(), (*color)[1].get<float>(), (*color)[2].get<float>()));
    }

    emitter->SetSpawnRate(data.value("spawnRate", emitter->GetSpawnRate()));
    emitter->SetParticleLifetime(data.value("lifetime", emitter->GetParticleLifetime()));
    emitter->SetParticleSpeed(data.value("speed", emitter->GetParticleSpeed()));
    emitter->SetParticleSize(data.value("size", emitter->GetParticleSize()));
    emitter->SetAlignToWind(data.value("alignToWind", emitter->GetAlignToWind()));
    emitter->SetProfileTag(data.value("profile", emitter->GetProfileTag()));

    if (auto max = data.find("maxParticles"); max != data.end() && max->is_object()) {
        emitter->SetMaxParticlesHigh(max->value("high", emitter->GetMaxParticlesHigh()));
        emitter->SetMaxParticlesMedium(max->value("medium", emitter->GetMaxParticlesMedium()));
        emitter->SetMaxParticlesLow(max->value("low", emitter->GetMaxParticlesLow()));
    }

    return emitter.get();
}

} // namespace

GM_REGISTER_COMPONENT_CUSTOM(WeatherEmitterComponent,
                             "WeatherEmitterComponent",
                             SerializeEmitter,
                             DeserializeEmitter);

} // namespace gm


