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

  // 3 floats pro Vertex (x,y,z)
  static Mesh fromPositions(const std::vector<float> &positions);
  static Mesh fromIndexed(const std::vector<float> &positions, const std::vector<unsigned int> &indices);

  void draw() const;

private:
  GLuint m_vao{0};
  GLuint m_vbo{0};
  GLuint m_ebo{0};          // optional (nur bei indexed)
  GLsizei m_vertexCount{0}; // für drawArrays
  GLsizei m_indexCount{0};  // für drawElements
  bool m_indexed{false};
};
