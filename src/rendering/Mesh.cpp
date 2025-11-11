#include "gm/rendering/Mesh.hpp"
#include <cstddef> // for size_t

namespace gm {

Mesh::~Mesh() {
    if (EBO)
        glDeleteBuffers(1, &EBO);
    if (VBO)
        glDeleteBuffers(1, &VBO);
    if (VAO)
        glDeleteVertexArrays(1, &VAO);
}

Mesh::Mesh(Mesh&& o) noexcept {
    VAO = o.VAO;
    o.VAO = 0;
    VBO = o.VBO;
    o.VBO = 0;
    EBO = o.EBO;
    o.EBO = 0;
    vertexCount = o.vertexCount;
    o.vertexCount = 0;
    hasIndices = o.hasIndices;
    o.hasIndices = false;
}
Mesh& Mesh::operator=(Mesh&& o) noexcept {
    if (this != &o) {
        this->~Mesh();
        VAO = o.VAO;
        o.VAO = 0;
        VBO = o.VBO;
        o.VBO = 0;
        EBO = o.EBO;
        o.EBO = 0;
        vertexCount = o.vertexCount;
        o.vertexCount = 0;
        hasIndices = o.hasIndices;
        o.hasIndices = false;
    }
    return *this;
}

// Helper function for attribute setup
static void setupAttributes() {
    const GLsizei stride = 8 * sizeof(float);
    // layout(location = 0) vec3 aPos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    // layout(location = 1) vec3 aNormal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    // layout(location = 2) vec2 aUV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
}

Mesh Mesh::fromPositions(const std::vector<float>& positions) {
    Mesh m;
    glGenVertexArrays(1, &m.VAO);
    glBindVertexArray(m.VAO);

    glGenBuffers(1, &m.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float), positions.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
    m.vertexCount = static_cast<int>(positions.size() / 3);
    return m;
}

Mesh Mesh::fromIndexed(const std::vector<float>& vertexData,
                      const std::vector<unsigned int>& indices) {
    Mesh m;
    glGenVertexArrays(1, &m.VAO);
    glBindVertexArray(m.VAO);

    glGenBuffers(1, &m.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    // Determine stride (supports position-only or position/normal/uv data)
    const std::size_t componentStrideFloats =
        (vertexData.size() % 8 == 0) ? 8 : 3;
    const GLsizei stride = static_cast<GLsizei>(componentStrideFloats * sizeof(float));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    if (componentStrideFloats >= 6) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    }
    if (componentStrideFloats >= 8) {
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    }

    glBindVertexArray(0);
    m.hasIndices = true;
    m.vertexCount = static_cast<int>(indices.size());
    return m;
}

void Mesh::Draw() const {
    if (!VAO)
        return;

    glBindVertexArray(VAO);
    if (hasIndices)
        glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, nullptr);
    else
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

void Mesh::DrawInstanced(unsigned int instanceCount) const {
    if (!VAO || instanceCount == 0)
        return;

    glBindVertexArray(VAO);
    if (hasIndices)
        glDrawElementsInstanced(GL_TRIANGLES, vertexCount, GL_UNSIGNED_INT, nullptr, instanceCount);
    else
        glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
    glBindVertexArray(0);
}

} // namespace gm
