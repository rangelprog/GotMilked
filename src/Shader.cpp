#include "gm/Shader.hpp"
#include <cstdio>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <vector>

namespace gm {

Shader::~Shader() {
    if (m_id)
        glDeleteProgram(m_id);
}

Shader::Shader(Shader&& other) noexcept {
    m_id = other.m_id;
    other.m_id = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_id)
            glDeleteProgram(m_id);
        m_id = other.m_id;
        other.m_id = 0;
    }
    return *this;
}

/*static*/ bool Shader::readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "Shader: failed to open file: %s\n", path.c_str());
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

GLuint Shader::compile(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1);
        glGetShaderInfoLog(id, len, nullptr, log.data());
        std::fprintf(stderr, "Shader: compile failed: %s\n", log.data());
        glDeleteShader(id);
        return 0;
    }
    return id;
}

GLuint Shader::link(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(static_cast<size_t>(len) + 1);
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        std::fprintf(stderr, "Shader: link failed: %s\n", log.data());
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool Shader::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    std::string vsCode, fsCode;
    if (!readFile(vertPath, vsCode) || !readFile(fragPath, fsCode))
        return false;

    GLuint vs = compile(GL_VERTEX_SHADER, vsCode.c_str());
    if (!vs)
        return false;
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsCode.c_str());
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    GLuint prog = link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog)
        return false;

    if (m_id)
        glDeleteProgram(m_id);
    m_id = prog;
    return true;
}

GLint Shader::uniformLoc(const char* name) const {
    return glGetUniformLocation(m_id, name);
}

void Shader::SetMat4(const char* name, const glm::mat4& m) const {
    const GLint loc = uniformLoc(name);
    if (loc >= 0)
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

void Shader::SetFloat(const char* name, float v) const {
    const GLint loc = uniformLoc(name);
    if (loc >= 0)
        glUniform1f(loc, v);
}

void Shader::SetInt(const char* name, int v) const {
    const GLint loc = uniformLoc(name);
    if (loc >= 0)
        glUniform1i(loc, v);
}

void Shader::SetVec3(const char* name, const glm::vec3& v) const {
    const GLint loc = uniformLoc(name);
    if (loc >= 0)
        glUniform3fv(loc, 1, glm::value_ptr(v));
}

void Shader::SetMat3(const char* name, const glm::mat3& m) const {
    const GLint loc = uniformLoc(name);
    if (loc >= 0)
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(m));
}

} // namespace gm