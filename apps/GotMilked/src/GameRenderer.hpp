#pragma once

#include "gm/rendering/CascadeShadowMap.hpp"
#include "SkyRenderer.hpp"
#include "VolumetricFogRenderer.hpp"
#include "WeatherParticleSystem.hpp"

class Game;

class GameRenderer {
public:
    explicit GameRenderer(Game& game);

    void Render();

private:
    void ProcessEnvironmentCaptureRequests();

    Game& m_game;
    gm::rendering::CascadeShadowMap m_shadowCascades;
    SkyRenderer m_skyRenderer;
    VolumetricFogRenderer m_fogRenderer;
    WeatherParticleSystem m_weatherParticles;
    bool m_skyInitialized = false;
    bool m_fogInitialized = false;
    bool m_weatherInitialized = false;
};

