#include "gm/rendering/Shader.hpp"
#include "gm/rendering/RenderStateCache.hpp"
#include "gm/core/Logger.hpp"
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <vector>
#include <cstring>

namespace gm {

void Shader::Use() const {
    RenderStateCache::BindShader(m_id);
}

Shader::UniformRecord::UniformRecord() {
    std::memset(&value, 0, sizeof(value));
}

Shader::~Shader() {
    if (m_id) {
        RenderStateCache::InvalidateShader(m_id);
        glDeleteProgram(m_id);
    }
}

Shader::Shader(Shader&& other) noexcept {
    m_id = other.m_id;
    m_uniformCache = std::move(other.m_uniformCache);
    other.m_id = 0;
    other.m_uniformCache.clear();
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_id) {
            RenderStateCache::InvalidateShader(m_id);
            glDeleteProgram(m_id);
        }
        m_id = other.m_id;
        m_uniformCache = std::move(other.m_uniformCache);
        other.m_id = 0;
        other.m_uniformCache.clear();
    }
    return *this;
}

/*static*/ bool Shader::readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        core::Logger::Error("Shader: failed to open file: {}", path);
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
        core::Logger::Error("Shader: compile failed: {}", log.data());
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
        core::Logger::Error("Shader: link failed: {}", log.data());
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

bool Shader::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    std::string vsCode, fsCode;
    if (!readFile(vertPath, vsCode) || !readFile(fragPath, fsCode))
        return false;

    return loadFromSource(vsCode, fsCode);
}

bool Shader::loadFromSource(std::string_view vertexSrc, std::string_view fragmentSrc) {
    std::string vertex(vertexSrc);
    std::string fragment(fragmentSrc);

    GLuint vs = compile(GL_VERTEX_SHADER, vertex.c_str());
    if (!vs)
        return false;
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment.c_str());
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }

    GLuint prog = link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!prog)
        return false;

    if (m_id) {
        RenderStateCache::InvalidateShader(m_id);
        glDeleteProgram(m_id);
    }
    m_id = prog;

    ClearUniformCache();

    return true;
}

void Shader::ClearUniformCache() const {
    m_uniformCache.clear();
}

Shader::UniformRecord* Shader::GetUniformRecord(std::string_view name) const {
    if (!m_id || name.empty()) {
        return nullptr;
    }

    std::string key(name);
    auto it = m_uniformCache.find(key);
    if (it == m_uniformCache.end()) {
        UniformRecord record;
        record.location = glGetUniformLocation(m_id, key.c_str());
        it = m_uniformCache.emplace(std::move(key), record).first;
    }
    return &it->second;
}

GLint Shader::uniformLoc(const char* name) const {
    return uniformLoc(std::string_view(name));
}

GLint Shader::uniformLoc(std::string_view name) const {
    if (auto* record = GetUniformRecord(name)) {
        return record->location;
    }
    return -1;
}

void Shader::SetMat4(const char* name, const glm::mat4& m) const {
    auto* record = GetUniformRecord(name);
    if (!record || record->location < 0) {
        return;
    }

    const auto* data = glm::value_ptr(m);
    if (record->hasValue && record->type == UniformRecord::Type::Mat4 &&
        std::memcmp(record->value.mat4Value, data, sizeof(float) * 16) == 0) {
        return;
    }

    record->type = UniformRecord::Type::Mat4;
    std::memcpy(record->value.mat4Value, data, sizeof(float) * 16);
    record->hasValue = true;
    glUniformMatrix4fv(record->location, 1, GL_FALSE, data);
}

void Shader::SetFloat(const char* name, float v) const {
    auto* record = GetUniformRecord(name);
    if (!record || record->location < 0) {
        return;
    }

    if (record->hasValue && record->type == UniformRecord::Type::Float &&
        record->value.floatValue == v) {
        return;
    }

    record->type = UniformRecord::Type::Float;
    record->value.floatValue = v;
    record->hasValue = true;
    glUniform1f(record->location, v);
}

void Shader::SetInt(const char* name, int v) const {
    auto* record = GetUniformRecord(name);
    if (!record || record->location < 0) {
        return;
    }

    if (record->hasValue && record->type == UniformRecord::Type::Int &&
        record->value.intValue == v) {
        return;
    }

    record->type = UniformRecord::Type::Int;
    record->value.intValue = v;
    record->hasValue = true;
    glUniform1i(record->location, v);
}

void Shader::SetVec3(const char* name, const glm::vec3& v) const {
    auto* record = GetUniformRecord(name);
    if (!record || record->location < 0) {
        return;
    }

    const auto* data = glm::value_ptr(v);
    if (record->hasValue && record->type == UniformRecord::Type::Vec3 &&
        std::memcmp(record->value.vec3Value, data, sizeof(float) * 3) == 0) {
        return;
    }

    record->type = UniformRecord::Type::Vec3;
    std::memcpy(record->value.vec3Value, data, sizeof(float) * 3);
    record->hasValue = true;
    glUniform3fv(record->location, 1, data);
}

void Shader::SetVec4(const char* name, const glm::vec4& v) const {
    auto* record = GetUniformRecord(name);
    if (!record || record->location < 0) {
        return;
    }

    const auto* data = glm::value_ptr(v);
    if (record->hasValue && record->type == UniformRecord::Type::Vec4 &&
        std::memcmp(record->value.vec4Value, data, sizeof(float) * 4) == 0) {
        return;
    }

    record->type = UniformRecord::Type::Vec4;
    std::memcpy(record->value.vec4Value, data, sizeof(float) * 4);
    record->hasValue = true;
    glUniform4fv(record->location, 1, data);
}

void Shader::SetMat3(const char* name, const glm::mat3& m) const {
    auto* record = GetUniformRecord(name);
    if (!record || record->location < 0) {
        return;
    }

    const auto* data = glm::value_ptr(m);
    if (record->hasValue && record->type == UniformRecord::Type::Mat3 &&
        std::memcmp(record->value.mat3Value, data, sizeof(float) * 9) == 0) {
        return;
    }

    record->type = UniformRecord::Type::Mat3;
    std::memcpy(record->value.mat3Value, data, sizeof(float) * 9);
    record->hasValue = true;
    glUniformMatrix3fv(record->location, 1, GL_FALSE, data);
}

} // namespace gm