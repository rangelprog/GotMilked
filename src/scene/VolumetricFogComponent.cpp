#include "gm/scene/VolumetricFogComponent.hpp"

#include "gm/scene/GameObject.hpp"
#include "gm/scene/ComponentRegistration.hpp"
#include <nlohmann/json.hpp>

namespace gm {
namespace {

nlohmann::json SerializeFog(Component* component) {
    auto* fog = static_cast<VolumetricFogComponent*>(component);
    nlohmann::json data;
    data["density"] = fog->GetDensity();
    data["heightFalloff"] = fog->GetHeightFalloff();
    data["maxDistance"] = fog->GetMaxDistance();
    data["color"] = { fog->GetColor().r, fog->GetColor().g, fog->GetColor().b };
    data["noiseScale"] = fog->GetNoiseScale();
    data["noiseSpeed"] = fog->GetNoiseSpeed();
    data["enabled"] = fog->IsEnabled();
    return data;
}

Component* DeserializeFog(GameObject* owner, const nlohmann::json& data) {
    auto fog = owner->AddComponent<VolumetricFogComponent>();
    fog->SetDensity(data.value("density", fog->GetDensity()));
    fog->SetHeightFalloff(data.value("heightFalloff", fog->GetHeightFalloff()));
    fog->SetMaxDistance(data.value("maxDistance", fog->GetMaxDistance()));
    fog->SetNoiseScale(data.value("noiseScale", fog->GetNoiseScale()));
    fog->SetNoiseSpeed(data.value("noiseSpeed", fog->GetNoiseSpeed()));
    fog->SetEnabled(data.value("enabled", fog->IsEnabled()));
    if (auto it = data.find("color"); it != data.end() && it->is_array() && it->size() == 3) {
        glm::vec3 color((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>());
        fog->SetColor(color);
    }
    return fog.get();
}

} // namespace

GM_REGISTER_COMPONENT_CUSTOM(VolumetricFogComponent,
                             "VolumetricFogComponent",
                             SerializeFog,
                             DeserializeFog);

} // namespace gm


