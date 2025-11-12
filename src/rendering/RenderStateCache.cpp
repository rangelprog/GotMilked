#include "gm/rendering/RenderStateCache.hpp"

#include <array>
#include <cassert>

namespace gm {
namespace {
constexpr int kMaxTrackedTextureUnits = 32;

struct TextureBinding {
    GLenum target = GL_TEXTURE_2D;
    GLuint texture = 0;
};

struct CacheState {
    GLuint currentProgram = 0;
    int activeTextureUnit = -1;
    std::array<TextureBinding, kMaxTrackedTextureUnits> textures{};

    void Reset() {
        currentProgram = 0;
        activeTextureUnit = -1;
        for (auto& binding : textures) {
            binding.target = GL_TEXTURE_2D;
            binding.texture = 0;
        }
    }
};

thread_local CacheState g_cache;

void EnsureTextureUnitActive(int unit) {
    if (g_cache.activeTextureUnit != unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        g_cache.activeTextureUnit = unit;
    }
}

bool IsTrackedUnit(int unit) {
    return unit >= 0 && unit < kMaxTrackedTextureUnits;
}
} // namespace

void RenderStateCache::BindShader(GLuint program) {
    if (program == 0) {
        glUseProgram(0);
        g_cache.currentProgram = 0;
        return;
    }
    if (g_cache.currentProgram == program) {
        return;
    }
    glUseProgram(program);
    g_cache.currentProgram = program;
}

void RenderStateCache::BindTexture(GLenum target, GLuint texture, int unit) {
    if (!IsTrackedUnit(unit)) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(target, texture);
        g_cache.activeTextureUnit = -1;
        return;
    }

    auto& binding = g_cache.textures[unit];
    if (binding.texture == texture && binding.target == target) {
        return;
    }

    EnsureTextureUnitActive(unit);
    glBindTexture(target, texture);
    binding.target = target;
    binding.texture = texture;
}

void RenderStateCache::InvalidateShader(GLuint program) {
    if (program == 0) {
        return;
    }
    if (g_cache.currentProgram == program) {
        g_cache.currentProgram = 0;
    }
}

void RenderStateCache::InvalidateTexture(GLuint texture) {
    if (texture == 0) {
        return;
    }
    for (auto& binding : g_cache.textures) {
        if (binding.texture == texture) {
            binding.texture = 0;
            binding.target = GL_TEXTURE_2D;
        }
    }
}

void RenderStateCache::Reset() {
    g_cache.Reset();
}

} // namespace gm

