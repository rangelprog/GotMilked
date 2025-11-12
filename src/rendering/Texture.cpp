#include "gm/rendering/Texture.hpp"
#include <glad/glad.h>
#include "gm/core/Error.hpp"
#include "gm/core/Logger.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "stb_image.h"

namespace gm {

Texture::~Texture() {
    if (m_id)
        glDeleteTextures(1, &m_id);
}

Texture::Texture(Texture&& o) noexcept {
    m_id = o.m_id;
    o.m_id = 0;
}

Texture& Texture::operator=(Texture&& o) noexcept {
  if (this != &o) {
    if (m_id)
      glDeleteTextures(1, &m_id);
    m_id = o.m_id;
    o.m_id = 0;
  }
  return *this;
}

bool Texture::createRGBA8(int width, int height,
                          const std::vector<std::uint8_t> &pixels,
                          bool generateMipmaps) {
  if (width <= 0 || height <= 0 || (int)pixels.size() < width * height * 4) {
    core::Logger::Error("Texture: invalid RGBA8 buffer (%dx%d, size=%zu)",
                        width, height, pixels.size());
    return false;
  }
  if (!m_id)
    glGenTextures(1, &m_id);
  glBindTexture(GL_TEXTURE_2D, m_id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels.data());

  if (generateMipmaps)
    glGenerateMipmap(GL_TEXTURE_2D);
  return true;
}

void Texture::bind(int unit) const {
    if (!m_id)
        return;
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

Texture Texture::makeChecker(int w, int h, int cell) {
    std::vector<std::uint8_t> px(w * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool on = ((x / cell) + (y / cell)) % 2 == 0;
            std::uint8_t c = on ? 240 : 30;
            size_t idx = static_cast<size_t>(y * w + x) * 4;
            px[idx + 0] = c;
            px[idx + 1] = c;
            px[idx + 2] = c;
            px[idx + 3] = 255;
        }
    }
    Texture t;
    if (!t.createRGBA8(w, h, px, true)) {
        throw gm::core::GraphicsError("texture.makeChecker", "Failed to allocate GPU texture");
    }
    return t;
}

Texture Texture::loadOrThrow(const std::string& path, bool flipY) {
    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!data) {
        std::string reason = stbi_failure_reason() ? stbi_failure_reason() : "unknown";
        throw gm::core::GraphicsError("texture.load", std::string("Failed to load ") + path + ": " + reason);
    }

    std::vector<std::uint8_t> pixels(static_cast<size_t>(w * h * 4));
    std::memcpy(pixels.data(), data, pixels.size());
    stbi_image_free(data);

    Texture t;
    if (!t.createRGBA8(w, h, pixels, true)) {
        throw gm::core::GraphicsError("texture.upload", std::string("Failed to upload ") + path);
    }
    return t;
}

} // namespace gm
