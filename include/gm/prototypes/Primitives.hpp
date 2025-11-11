#pragma once

#include "gm/rendering/Mesh.hpp"

namespace gm::prototypes {

/**
 * @brief Create a rectangular plane aligned with the XZ plane and centered at the origin.
 * @param width Plane size along the X axis.
 * @param depth Plane size along the Z axis.
 * @param uvScale Repeats UV coordinates across the plane.
 */
Mesh CreatePlane(float width, float depth, float uvScale = 1.0f);

/**
 * @brief Create a unit cube centered at the origin.
 * @param size Length of each cube edge.
 */
Mesh CreateCube(float size = 1.0f);

} // namespace gm::prototypes


