#pragma once
#include <glad/glad.h>
#include <vector>

class Mesh {
public:
  Mesh() = default;
  ~Mesh();

  Mesh(const Mesh &) = delete;
  Mesh &operator=(const Mesh &) = delete;
  Mesh(Mesh &&other) noexcept;
  Mesh &operator=(Mesh &&other) noexcept;

  // Position-only (3 floats/vertex)
  static Mesh fromPositions(const std::vector<float> &positions);

  // Position-only + Indizes
  static Mesh fromIndexed(const std::vector<float> &positions,
                          const std::vector<unsigned int> &indices);

  // Position + UV (separate arrays) + Indizes
  // positions: 3 floats/vertex, uvs: 2 floats/vertex
  static Mesh fromIndexedPUV(const std::vector<float> &positions,
                             const std::vector<float> &uvs,
                             const std::vector<unsigned int> &indices);

  // ... dein Header oben bleibt gleich ...
  // Position + Normal + UV (separate Arrays) + Indizes
  // positions: 3 floats/vertex, normals: 3 floats/vertex, uvs: 2 floats/vertex
  static Mesh fromIndexedPNU(const std::vector<float> &positions,
                             const std::vector<float> &normals,
                             const std::vector<float> &uvs,
                             const std::vector<unsigned int> &indices);

  void draw() const;

private:
  GLuint m_vao{0};
  GLuint m_vbo{0};
  GLuint m_ebo{0};
  GLsizei m_vertexCount{0}; // drawArrays
  GLsizei m_indexCount{0};  // drawElements
  bool m_indexed{false};
  bool m_hasUV{false};
};
