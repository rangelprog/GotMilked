#include "Texture.hpp"
#include <cstdio>

Texture::~Texture() {
  if (m_id)
    glDeleteTextures(1, &m_id);
}

Texture::Texture(Texture &&o) noexcept {
  m_id = o.m_id;
  o.m_id = 0;
}
Texture &Texture::operator=(Texture &&o) noexcept {
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
    std::fprintf(stderr, "Texture: invalid data\n");
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
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, m_id);
}

Texture Texture::makeChecker(int w, int h, int cell) {
  std::vector<std::uint8_t> px(w * h * 4);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      bool on = ((x / cell) + (y / cell)) % 2 == 0;
      std::uint8_t c = on ? 240 : 30;
      int idx = (y * w + x) * 4;
      px[idx + 0] = c;
      px[idx + 1] = c;
      px[idx + 2] = c;
      px[idx + 3] = 255;
    }
  }
  Texture t;
  t.createRGBA8(w, h, px, true);
  return t;
}
