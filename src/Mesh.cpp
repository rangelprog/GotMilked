#include "Mesh.hpp"
#include <cstddef> // for size_t

Mesh::~Mesh() {
  if (m_ebo)
    glDeleteBuffers(1, &m_ebo);
  if (m_vbo)
    glDeleteBuffers(1, &m_vbo);
  if (m_vao)
    glDeleteVertexArrays(1, &m_vao);
}

Mesh::Mesh(Mesh &&o) noexcept {
  m_vao = o.m_vao;
  o.m_vao = 0;
  m_vbo = o.m_vbo;
  o.m_vbo = 0;
  m_ebo = o.m_ebo;
  o.m_ebo = 0;
  m_vertexCount = o.m_vertexCount;
  o.m_vertexCount = 0;
  m_indexCount = o.m_indexCount;
  o.m_indexCount = 0;
  m_indexed = o.m_indexed;
  o.m_indexed = false;
}
Mesh &Mesh::operator=(Mesh &&o) noexcept {
  if (this != &o) {
    this->~Mesh();
    m_vao = o.m_vao;
    o.m_vao = 0;
    m_vbo = o.m_vbo;
    o.m_vbo = 0;
    m_ebo = o.m_ebo;
    o.m_ebo = 0;
    m_vertexCount = o.m_vertexCount;
    o.m_vertexCount = 0;
    m_indexCount = o.m_indexCount;
    o.m_indexCount = 0;
    m_indexed = o.m_indexed;
    o.m_indexed = false;
  }
  return *this;
}

// --- bereits vorhandene fromPositions/fromIndexed bleiben unverändert ---

// Helper zum Attribut-Setup für P/N/UV
static void setupPNUVAttribs() {
  const GLsizei stride = 8 * sizeof(float);
  // layout(location = 0) vec3 aPos
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
  // layout(location = 1) vec3 aNormal
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));
  // layout(location = 2) vec2 aUV
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(6 * sizeof(float)));
}

Mesh Mesh::fromPNUV(const std::vector<float> &data) {
  Mesh m;
  glGenVertexArrays(1, &m.m_vao);
  glBindVertexArray(m.m_vao);

  glGenBuffers(1, &m.m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.m_vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(),
               GL_STATIC_DRAW);

  setupPNUVAttribs();

  glBindVertexArray(0);
  m.m_indexed = false;
  m.m_vertexCount =
      static_cast<GLsizei>(data.size() / 8); // 8 floats per vertex
  return m;
}

Mesh Mesh::fromIndexedPNUV(const std::vector<float> &data,
                           const std::vector<unsigned int> &indices) {
  Mesh m;
  glGenVertexArrays(1, &m.m_vao);
  glBindVertexArray(m.m_vao);

  glGenBuffers(1, &m.m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.m_vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m.m_ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  setupPNUVAttribs();

  glBindVertexArray(0);
  m.m_indexed = true;
  m.m_indexCount = static_cast<GLsizei>(indices.size());
  return m;
}

void Mesh::draw() const {
  if (!m_vao)
    return;

  glBindVertexArray(m_vao);
  if (m_indexed) {
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, (void *)0);
  } else {
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
  }
  glBindVertexArray(0);
}
