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

  // schon vorhanden:
  static Mesh fromPositions(const std::vector<float> &positions);
  static Mesh fromIndexed(const std::vector<float> &positions,
                          const std::vector<unsigned int> &indices);

  // >>> NEU: P/N/UV (8 floats pro Vertex: Px Py Pz Nx Ny Nz U V)
  static Mesh
  fromPNUV(const std::vector<float> &interleavedPNUV); // non-indexed
  static Mesh fromIndexedPNUV(const std::vector<float> &interleavedPNUV,
                              const std::vector<unsigned int> &indices);

  void draw() const;

private:
  GLuint m_vao{0};
  GLuint m_vbo{0};
  GLuint m_ebo{0};          // optional
  GLsizei m_vertexCount{0}; // für drawArrays
  GLsizei m_indexCount{0};  // für drawElements
  bool m_indexed{false};
};
