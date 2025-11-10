#include "Mesh.hpp"
#include <vector>

Mesh::~Mesh() {
  if (m_ebo)
    glDeleteBuffers(1, &m_ebo);
  if (m_vbo)
    glDeleteBuffers(1, &m_vbo);
  if (m_vao)
    glDeleteVertexArrays(1, &m_vao);
}

Mesh::Mesh(Mesh &&other) noexcept {
  m_vao = other.m_vao;
  other.m_vao = 0;
  m_vbo = other.m_vbo;
  other.m_vbo = 0;
  m_ebo = other.m_ebo;
  other.m_ebo = 0;
  m_vertexCount = other.m_vertexCount;
  other.m_vertexCount = 0;
  m_indexCount = other.m_indexCount;
  other.m_indexCount = 0;
  m_indexed = other.m_indexed;
  other.m_indexed = false;
  m_hasUV = other.m_hasUV;
  other.m_hasUV = false;
}

Mesh &Mesh::operator=(Mesh &&other) noexcept {
  if (this != &other) {
    if (m_ebo)
      glDeleteBuffers(1, &m_ebo);
    if (m_vbo)
      glDeleteBuffers(1, &m_vbo);
    if (m_vao)
      glDeleteVertexArrays(1, &m_vao);

    m_vao = other.m_vao;
    other.m_vao = 0;
    m_vbo = other.m_vbo;
    other.m_vbo = 0;
    m_ebo = other.m_ebo;
    other.m_ebo = 0;
    m_vertexCount = other.m_vertexCount;
    other.m_vertexCount = 0;
    m_indexCount = other.m_indexCount;
    other.m_indexCount = 0;
    m_indexed = other.m_indexed;
    other.m_indexed = false;
    m_hasUV = other.m_hasUV;
    other.m_hasUV = false;
  }
  return *this;
}

Mesh Mesh::fromPositions(const std::vector<float> &positions) {
  Mesh m;
  m.m_vertexCount = static_cast<GLsizei>(positions.size() / 3);

  glGenVertexArrays(1, &m.m_vao);
  glBindVertexArray(m.m_vao);

  glGenBuffers(1, &m.m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.m_vbo);
  glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float),
               positions.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

  glBindVertexArray(0);
  return m;
}

Mesh Mesh::fromIndexed(const std::vector<float> &positions,
                       const std::vector<unsigned int> &indices) {
  Mesh m;
  m.m_indexed = true;
  m.m_indexCount = static_cast<GLsizei>(indices.size());

  glGenVertexArrays(1, &m.m_vao);
  glBindVertexArray(m.m_vao);

  glGenBuffers(1, &m.m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.m_vbo);
  glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(float),
               positions.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m.m_ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

  glBindVertexArray(0);
  return m;
}

Mesh Mesh::fromIndexedPUV(const std::vector<float> &positions,
                          const std::vector<float> &uvs,
                          const std::vector<unsigned int> &indices) {
  const size_t vertCount = positions.size() / 3;

  Mesh m;
  m.m_indexed = true;
  m.m_hasUV = true;
  m.m_indexCount = static_cast<GLsizei>(indices.size());

  // Interleaved: [Px,Py,Pz, U, V] * N
  std::vector<float> interleaved;
  interleaved.reserve(vertCount * 5);
  for (size_t i = 0; i < vertCount; ++i) {
    interleaved.push_back(positions[i * 3 + 0]);
    interleaved.push_back(positions[i * 3 + 1]);
    interleaved.push_back(positions[i * 3 + 2]);
    interleaved.push_back(uvs[i * 2 + 0]);
    interleaved.push_back(uvs[i * 2 + 1]);
  }

  glGenVertexArrays(1, &m.m_vao);
  glBindVertexArray(m.m_vao);

  glGenBuffers(1, &m.m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.m_vbo);
  glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float),
               interleaved.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m.m_ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  const GLsizei stride = 5 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));

  glBindVertexArray(0);
  return m;
}

Mesh Mesh::fromIndexedPNU(const std::vector<float> &positions,
                          const std::vector<float> &normals,
                          const std::vector<float> &uvs,
                          const std::vector<unsigned int> &indices) {
  const size_t vertCount = positions.size() / 3;

  Mesh m;
  m.m_indexed = true;
  m.m_hasUV = true;
  m.m_indexCount = static_cast<GLsizei>(indices.size());

  // Interleave: [Px,Py,Pz, Nx,Ny,Nz, U, V] * N  -> stride = 8 floats
  std::vector<float> interleaved;
  interleaved.reserve(vertCount * 8);
  for (size_t i = 0; i < vertCount; ++i) {
    interleaved.push_back(positions[i * 3 + 0]);
    interleaved.push_back(positions[i * 3 + 1]);
    interleaved.push_back(positions[i * 3 + 2]);

    interleaved.push_back(normals[i * 3 + 0]);
    interleaved.push_back(normals[i * 3 + 1]);
    interleaved.push_back(normals[i * 3 + 2]);

    interleaved.push_back(uvs[i * 2 + 0]);
    interleaved.push_back(uvs[i * 2 + 1]);
  }

  glGenVertexArrays(1, &m.m_vao);
  glBindVertexArray(m.m_vao);

  glGenBuffers(1, &m.m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.m_vbo);
  glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float),
               interleaved.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m.m_ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);

  const GLsizei stride = 8 * sizeof(float);
  // location 0: position (vec3)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

  // location 1: normal (vec3)  offset = 3 floats
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));

  // location 2: uv (vec2)      offset = 6 floats
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(6 * sizeof(float)));

  glBindVertexArray(0);
  return m;
}

Mesh Mesh::fromPNUV_Quad() {
  const float n[3] = {0.0f, 0.0f, 1.0f};

  std::vector<float> data = {// Px, Py, Pz,   Nx, Ny, Nz,   U, V
                             -0.5f, -0.5f, 0.0f, n[0], n[1], n[2], 0.0f, 0.0f,
                             0.5f,  -0.5f, 0.0f, n[0], n[1], n[2], 1.0f, 0.0f,
                             0.5f,  0.5f,  0.0f, n[0], n[1], n[2], 1.0f, 1.0f,
                             -0.5f, 0.5f,  0.0f, n[0], n[1], n[2], 0.0f, 1.0f};

  std::vector<unsigned int> idx = {0, 1, 2, 2, 3, 0};

  Mesh m;

  glGenVertexArrays(1, &m.m_vao);
  glBindVertexArray(m.m_vao);

  glGenBuffers(1, &m.m_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.m_vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m.m_ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int),
               idx.data(), GL_STATIC_DRAW);

  const GLsizei stride = 8 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));

  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(6 * sizeof(float)));

  m.m_indexed = true;
  m.m_indexCount = 6;

  glBindVertexArray(0);
  return m;
}

void Mesh::draw() const {
  glBindVertexArray(m_vao);
  if (m_indexed) {
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, (void *)0);
  } else {
    glDrawArrays(GL_TRIANGLES, 0, m_vertexCount);
  }
}
