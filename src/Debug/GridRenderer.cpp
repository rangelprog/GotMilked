#include "gm/debug/GridRenderer.hpp"

#if GM_DEBUG_TOOLS

#include <cstddef>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/vec4.hpp>

#include "gm/core/Logger.hpp"

namespace {
constexpr int kGridHalfCells = 250;
constexpr float kCellSize = 1.0f;
constexpr int kMajorStep = 10;
constexpr float kAxisHeight = 20.0f;
}

namespace gm::debug {

GridRenderer::~GridRenderer() {
    Release();
}

bool GridRenderer::Initialize() {
    if (m_initialized)
        return true;

    static constexpr char kVertexShader[] = R"(#version 460 core
layout(location = 0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
)";

    static constexpr char kFragmentShader[] = R"(#version 460 core
uniform vec4 uColor;

out vec4 FragColor;

void main() {
    FragColor = uColor;
}
)";

    if (!m_shader.loadFromSource(kVertexShader, kFragmentShader)) {
        gm::core::Logger::Error("[GridRenderer] Failed to compile shaders");
        Release();
        return false;
    }

    m_viewUniform = m_shader.uniformLoc("uView");
    m_projectionUniform = m_shader.uniformLoc("uProjection");
    m_colorUniform = m_shader.uniformLoc("uColor");

    const float extent = static_cast<float>(kGridHalfCells) * kCellSize;

    std::vector<glm::vec3> minorLines;
    std::vector<glm::vec3> majorLines;
    minorLines.reserve(static_cast<std::size_t>(kGridHalfCells * 8));
    majorLines.reserve(static_cast<std::size_t>(kGridHalfCells * 2));

    for (int i = -kGridHalfCells; i <= kGridHalfCells; ++i) {
        const float position = static_cast<float>(i) * kCellSize;
        const bool isAxis = (i == 0);
        const bool isMajor = (i % kMajorStep) == 0;

        if (isAxis) {
            continue;
        }

        auto& target = isMajor ? majorLines : minorLines;
        target.emplace_back(-extent, 0.0f, position);
        target.emplace_back(extent, 0.0f, position);
        target.emplace_back(position, 0.0f, -extent);
        target.emplace_back(position, 0.0f, extent);
    }

    std::vector<glm::vec3> axisX = {
        {-extent, 0.0f, 0.0f},
        {extent, 0.0f, 0.0f},
    };
    std::vector<glm::vec3> axisZ = {
        {0.0f, 0.0f, -extent},
        {0.0f, 0.0f, extent},
    };
    std::vector<glm::vec3> verticalAxis = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, kAxisHeight, 0.0f},
    };

    m_minorLines = CreateBuffer(minorLines);
    m_majorLines = CreateBuffer(majorLines);
    m_axisX = CreateBuffer(axisX);
    m_axisZ = CreateBuffer(axisZ);
    m_verticalAxis = CreateBuffer(verticalAxis);

    if (!m_minorLines.vertexCount && !m_majorLines.vertexCount) {
        gm::core::Logger::Warning("[GridRenderer] Generated grid has no line data");
    }

    m_initialized = true;
    return true;
}

void GridRenderer::Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    if (!m_initialized && !Initialize()) {
        return;
    }
    if (!m_initialized) {
        return;
    }

    m_shader.Use();

    if (m_viewUniform >= 0) {
        glUniformMatrix4fv(m_viewUniform, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    }
    if (m_projectionUniform >= 0) {
        glUniformMatrix4fv(m_projectionUniform, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    }

    GLboolean depthMask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLfloat previousLineWidth = 1.0f;
    glGetFloatv(GL_LINE_WIDTH, &previousLineWidth);

    glDepthMask(GL_FALSE);
    if (!blendEnabled) {
        glEnable(GL_BLEND);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto drawLines = [&](const LineBuffer& buffer, const glm::vec4& color) {
        if (!buffer.vertexCount) {
            return;
        }
        if (m_colorUniform >= 0) {
            glUniform4fv(m_colorUniform, 1, glm::value_ptr(color));
        }
        glBindVertexArray(buffer.vao);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(buffer.vertexCount));
    };

    const glm::vec4 minorColor{0.35f, 0.35f, 0.38f, 0.25f};
    const glm::vec4 majorColor{0.45f, 0.45f, 0.50f, 0.45f};
    const glm::vec4 axisColorX{0.85f, 0.25f, 0.25f, 0.85f};
    const glm::vec4 axisColorZ{0.25f, 0.45f, 0.85f, 0.85f};
    const glm::vec4 verticalColor{0.95f, 0.95f, 0.95f, 0.6f};

    glLineWidth(1.0f);
    drawLines(m_minorLines, minorColor);
    glLineWidth(1.25f);
    drawLines(m_majorLines, majorColor);
    glLineWidth(1.5f);
    drawLines(m_axisX, axisColorX);
    drawLines(m_axisZ, axisColorZ);
    glLineWidth(2.0f);
    drawLines(m_verticalAxis, verticalColor);

    glBindVertexArray(0);
    glLineWidth(previousLineWidth);
    if (!blendEnabled) {
        glDisable(GL_BLEND);
    }
    glDepthMask(depthMask);
}

GridRenderer::LineBuffer GridRenderer::CreateBuffer(const std::vector<glm::vec3>& vertices) {
    LineBuffer buffer;
    if (vertices.empty()) {
        return buffer;
    }

    glGenVertexArrays(1, &buffer.vao);
    glGenBuffers(1, &buffer.vbo);
    glBindVertexArray(buffer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glBindVertexArray(0);

    buffer.vertexCount = static_cast<unsigned int>(vertices.size());
    return buffer;
}

void GridRenderer::DestroyBuffer(LineBuffer& buffer) {
    if (buffer.vbo) {
        glDeleteBuffers(1, &buffer.vbo);
        buffer.vbo = 0;
    }
    if (buffer.vao) {
        glDeleteVertexArrays(1, &buffer.vao);
        buffer.vao = 0;
    }
    buffer.vertexCount = 0;
}

void GridRenderer::Release() {
    DestroyBuffer(m_minorLines);
    DestroyBuffer(m_majorLines);
    DestroyBuffer(m_axisX);
    DestroyBuffer(m_axisZ);
    DestroyBuffer(m_verticalAxis);
    m_initialized = false;
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


