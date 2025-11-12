#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <string_view>

namespace gm {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);

    void Use() const { glUseProgram(m_id); }
    GLuint Id() const { return m_id; }

    GLint uniformLoc(const char* name) const;
    GLint uniformLoc(std::string_view name) const;

    void SetInt(const char* name, int value) const;
    void SetFloat(const char* name, float value) const;
    void SetVec3(const char* name, const glm::vec3& value) const;
    void SetMat3(const char* name, const glm::mat3& mat) const;
    void SetMat4(const char* name, const glm::mat4& mat) const;

private:
    static bool readFile(const std::string& path, std::string& out);
    static GLuint compile(GLenum type, const char* src);
    static GLuint link(GLuint vs, GLuint fs);
    
    // Clear uniform cache (called when shader is reloaded)
    void ClearUniformCache() const;
    struct UniformRecord;
    UniformRecord* GetUniformRecord(std::string_view name) const;

private:
    struct UniformRecord {
        enum class Type : unsigned char {
            Unknown = 0,
            Int,
            Float,
            Vec3,
            Mat3,
            Mat4
        };

        UniformRecord();

        GLint location = -1;
        Type type = Type::Unknown;
        bool hasValue = false;
        union {
            int intValue;
            float floatValue;
            float vec3Value[3];
            float mat3Value[9];
            float mat4Value[16];
        } value;
    };

    GLuint m_id{0};
    // Uniform location cache: uniform name -> location
    // Using mutable because cache lookup doesn't change logical state
    mutable std::unordered_map<std::string, UniformRecord> m_uniformCache;
};

}