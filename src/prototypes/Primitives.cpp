#include "gm/prototypes/Primitives.hpp"

#include <array>
#include <vector>

namespace gm::prototypes {

Mesh CreatePlane(float width, float depth, float uvScale) {
    const float halfWidth = width * 0.5f;
    const float halfDepth = depth * 0.5f;

    // Position, normal, UV (8 floats per vertex)
    const std::array<float, 32> vertices = {
        // Bottom left
        -halfWidth, 0.0f, -halfDepth, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        // Bottom right
         halfWidth, 0.0f, -halfDepth, 0.0f, 1.0f, 0.0f, uvScale, 0.0f,
        // Top right
         halfWidth, 0.0f,  halfDepth, 0.0f, 1.0f, 0.0f, uvScale, uvScale,
        // Top left
        -halfWidth, 0.0f,  halfDepth, 0.0f, 1.0f, 0.0f, 0.0f, uvScale
    };

    const std::array<unsigned int, 6> indices = {
        0, 1, 2,
        0, 2, 3
    };

    return Mesh::fromIndexed(
        std::vector<float>(vertices.begin(), vertices.end()),
        std::vector<unsigned int>(indices.begin(), indices.end()));
}

Mesh CreateCube(float size) {
    const float h = size * 0.5f;

    // 36 vertices (6 faces * 2 triangles * 3 vertices)
    const std::vector<float> vertices = {
        // Front face
        -h, -h,  h,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         h, -h,  h,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
         h,  h,  h,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
        -h, -h,  h,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         h,  h,  h,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
        -h,  h,  h,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,

        // Back face
        -h, -h, -h,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
        -h,  h, -h,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
         h,  h, -h,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
        -h, -h, -h,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
         h,  h, -h,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
         h, -h, -h,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,

        // Left face
        -h, -h, -h, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
        -h, -h,  h, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
        -h,  h,  h, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
        -h, -h, -h, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
        -h,  h,  h, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
        -h,  h, -h, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,

        // Right face
         h, -h, -h,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
         h,  h,  h,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
         h, -h,  h,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
         h, -h, -h,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
         h,  h, -h,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
         h,  h,  h,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,

        // Top face
        -h,  h,  h,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f,
         h,  h,  h,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
         h,  h, -h,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
        -h,  h,  h,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f,
         h,  h, -h,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
        -h,  h, -h,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,

        // Bottom face
        -h, -h,  h,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
        -h, -h, -h,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
         h, -h, -h,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
        -h, -h,  h,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
         h, -h, -h,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
         h, -h,  h,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
    };

    // Sequential indices since data already expanded per-triangle
    std::vector<unsigned int> indices(vertices.size() / 8);
    for (unsigned int i = 0; i < indices.size(); ++i) {
        indices[i] = i;
    }

    return Mesh::fromIndexed(vertices, indices);
}

} // namespace gm::prototypes


