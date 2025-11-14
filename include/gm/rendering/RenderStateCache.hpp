#pragma once

#include <glad/glad.h>

namespace gm {

class RenderStateCache {
public:
    static void BindShader(GLuint program);
    static void BindTexture(GLenum target, GLuint texture, int unit);
    static void BindUniformBuffer(GLuint buffer, GLuint bindingPoint);

    static void InvalidateShader(GLuint program);
    static void InvalidateTexture(GLuint texture);
    static void InvalidateUniformBuffer(GLuint buffer);

    static void Reset();
};

} // namespace gm

