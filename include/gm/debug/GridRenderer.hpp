#pragma once

#if GM_DEBUG_TOOLS

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>

#include "gm/rendering/Shader.hpp"

namespace gm::debug {

class GridRenderer {
public:
    GridRenderer() = default;
    ~GridRenderer();

    GridRenderer(const GridRenderer&) = delete;
    GridRenderer& operator=(const GridRenderer&) = delete;

    bool Initialize();
    void Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

private:
    struct LineBuffer {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int vertexCount = 0;
    };

    LineBuffer CreateBuffer(const std::vector<glm::vec3>& vertices);
    void DestroyBuffer(LineBuffer& buffer);
    void Release();

    bool m_initialized = false;
    gm::Shader m_shader;
    LineBuffer m_minorLines;
    LineBuffer m_majorLines;
    LineBuffer m_axisX;
    LineBuffer m_axisZ;
    LineBuffer m_verticalAxis;
    int m_viewUniform = -1;
    int m_projectionUniform = -1;
    int m_colorUniform = -1;
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


