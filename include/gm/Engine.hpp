#pragma once

// Core Systems
#include "gm/core/Event.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Time.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/InputBindings.hpp"
#include "gm/core/Error.hpp"

// Rendering
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Texture.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/LightManager.hpp"

// Animation
#include "gm/animation/Skeleton.hpp"
#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/SkinnedMeshAsset.hpp"

// Scene Management
#include "gm/scene/Component.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/Transform.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/MaterialComponent.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/ComponentFactory.hpp"
#include "gm/scene/StaticMeshComponent.hpp"

// Utilities
#include "gm/utils/ObjLoader.hpp"
#include "gm/utils/ResourceManager.hpp"
#include "gm/utils/CoordinateDisplay.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/utils/FileDialog.hpp"

// Prototypes
#include "gm/prototypes/Primitives.hpp"

// Physics
#include "gm/physics/PhysicsWorld.hpp"
#include "gm/physics/RigidBodyComponent.hpp"

// Gameplay
#include "gm/gameplay/FlyCameraController.hpp"

// Save / Tooling
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/tooling/Overlay.hpp"

// Animation Components
#include "gm/scene/SkinnedMeshComponent.hpp"
#include "gm/scene/AnimatorComponent.hpp"