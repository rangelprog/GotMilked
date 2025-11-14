#pragma once

// This header intentionally exposes only the primary runtime entry points.
// Include the specific module headers you need (core, rendering, scene, etc.)
// instead of relying on this façade to pull in the entire engine.

// Core application bootstrap
#include "gm/core/GameApp.hpp"
#include "gm/core/Event.hpp"
#include "gm/core/Logger.hpp"

// Scene lifecycle management
#include "gm/scene/Scene.hpp"
#include "gm/scene/SceneManager.hpp"

// Resource & hot reload services
#include "gm/utils/ResourceManager.hpp"
#include "gm/utils/HotReloader.hpp"

// Physics world access
#include "gm/physics/PhysicsWorld.hpp"