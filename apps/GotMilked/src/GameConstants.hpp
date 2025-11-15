#pragma once

#include <glm/vec3.hpp>

namespace gotmilked {

/**
 * @brief Game-wide constants for configuration values
 * 
 * This file centralizes all magic numbers used throughout the game code,
 * making them easier to modify and understand.
 */
namespace GameConstants {

// ============================================================================
// Terrain Constants
// ============================================================================
namespace Terrain {
    constexpr int DefaultResolution = 33;
    constexpr float DefaultSize = 20.0f;
    constexpr float InitialSize = 40.0f;  // Used when creating initial terrain
    constexpr float DefaultMinHeight = -2.0f;
    constexpr float DefaultMaxHeight = 4.0f;
    constexpr float DefaultBrushRadius = 1.5f;
    constexpr float DefaultBrushStrength = 1.0f;
    
    // Brush limits
    constexpr float MinBrushRadius = 0.1f;
    constexpr float MaxBrushRadius = 20.0f;
    constexpr float MinBrushStrength = 0.01f;
    constexpr float MaxBrushStrength = 20.0f;
    
    // Height limits
    constexpr float MinHeightLimit = -10.0f;
    constexpr float MaxHeightLimit = 10.0f;
    
    // UI slider ranges
    constexpr float BrushRadiusSliderMin = 0.5f;
    constexpr float BrushRadiusSliderMax = 5.0f;
    constexpr float BrushStrengthSliderMin = 0.1f;
    constexpr float BrushStrengthSliderMax = 5.0f;
    constexpr float MinHeightSliderMin = -10.0f;
    constexpr float MinHeightSliderMax = 0.0f;
    constexpr float MaxHeightSliderMin = 0.5f;
    constexpr float MaxHeightSliderMax = 10.0f;
} // namespace Terrain

// ============================================================================
// Camera Constants
// ============================================================================
namespace Camera {
    constexpr float DefaultFovDegrees = 60.0f;
    constexpr float NearPlane = 0.1f;
    constexpr float FarPlane = 200.0f;
} // namespace Camera

// ============================================================================
// Material Constants
// ============================================================================
namespace Material {
    // Default material properties
    constexpr float DefaultShininess = 32.0f;
    constexpr float PlaneShininess = 16.0f;
    constexpr glm::vec3 DefaultSpecular = glm::vec3(0.5f);
    
    // Plane material (ground)
    constexpr glm::vec3 PlaneDiffuse = glm::vec3(0.2f, 0.5f, 0.2f);
    
    // Cube material
    constexpr glm::vec3 CubeDiffuse = glm::vec3(0.75f, 0.2f, 0.2f);
} // namespace Material

// ============================================================================
// Mesh Constants
// ============================================================================
namespace Mesh {
    // Plane mesh dimensions
    constexpr float PlaneWidth = 50.0f;
    constexpr float PlaneHeight = 50.0f;
    constexpr float PlaneSubdivisions = 10.0f;
    
    // Cube mesh size
    constexpr float CubeSize = 1.5f;
} // namespace Mesh

// ============================================================================
// Light Constants
// ============================================================================
namespace Light {
    // Sun light properties
    constexpr glm::vec3 SunPosition = glm::vec3(0.0f, 10.0f, 0.0f);
    constexpr glm::vec3 SunDirection = glm::vec3(-0.4f, -1.0f, -0.3f);
    constexpr glm::vec3 SunColor = glm::vec3(1.0f);
    constexpr float SunIntensity = 1.5f;

    constexpr glm::vec3 MoonColor = glm::vec3(0.5f, 0.6f, 1.0f);
    constexpr float MoonIntensity = 0.2f;
} // namespace Light

// ============================================================================
// Rendering Constants
// ============================================================================
namespace Rendering {
    // Screen space calculations
    constexpr float NdcToScreenScale = 0.5f;
    constexpr float NdcOffset = 1.0f;
    constexpr float LabelOffsetY = 4.0f;
    
    // GameObject label rendering
    constexpr float DotSize = 8.0f;
    constexpr float LabelTextOffset = 0.5f;
} // namespace Rendering

// ============================================================================
// Transform Constants
// ============================================================================
namespace Transform {
    constexpr glm::vec3 Origin = glm::vec3(0.0f, 0.0f, 0.0f);
    constexpr glm::vec3 UnitScale = glm::vec3(1.0f);
} // namespace Transform

} // namespace GameConstants

} // namespace gotmilked

