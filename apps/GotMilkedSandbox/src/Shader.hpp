#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
  Shader() = default;
  ~Shader();

  Shader(const Shader &) = delete;
  Shader &operator=(const Shader &) = delete;
  Shader(Shader &&other) noexcept;
  Shader &operator=(Shader &&other) noexcept;

  // Load, compile, link from files. Returns true on success.
  bool loadFromFiles(const std::string &vertPath, const std::string &fragPath);

  void use() const { glUseProgram(m_id); }
  GLuint id() const { return m_id; }

  // Uniform helpers
  GLint uniformLoc(const char *name) const;
  void setMat4(const char *name, const glm::mat4 &m) const;
  void setFloat(const char *name, float v) const;
  void setInt(const char *name, int v) const;

private:
  static bool readFile(const std::string &path, std::string &out);
  static GLuint compile(GLenum type, const char *src);
  static GLuint link(GLuint vs, GLuint fs);

  GLuint m_id{0};
};
