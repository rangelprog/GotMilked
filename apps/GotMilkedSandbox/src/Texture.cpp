#include "Texture.hpp"
#include <cstdio>
#include <vector>

// nur deklarieren; Implementierung ist in stb_image_impl.cpp
extern "C" {
int stbi_info_from_memory(const unsigned char *, int, int *, int *, int *);
void stbi_set_flip_vertically_on_load(int flag_true_if_should_flip);
unsigned char *stbi_load(const char *filename, int *x, int *y, int *comp,
                         int req_comp);
unsigned char *stbi_load_from_memory(const unsigned char *buffer, int len,
                                     int *x, int *y, int *comp, int req_comp);
void stbi_image_free(void *retval_from_stbi_load);
}

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
  if (width <= 0 || height <= 0 ||
      static_cast<int>(pixels.size()) < width * height * 4) {
    std::fprintf(stderr, "Texture: invalid RGBA8 data\n");
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

bool Texture::load(const std::string &path, bool flipY, bool generateMipmaps) {
  int w = 0, h = 0, comp = 0;
  stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

  unsigned char *data = stbi_load(path.c_str(), &w, &h, &comp, 4); // force RGBA
  if (!data) {
    std::fprintf(stderr, "Texture: failed to load image: %s\n", path.c_str());
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               data);

  if (generateMipmaps)
    glGenerateMipmap(GL_TEXTURE_2D);

  stbi_image_free(data);
  return true;
}

void Texture::bind(int unit) const {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, m_id);
}

Texture Texture::makeChecker(int w, int h, int cell) {
  std::vector<std::uint8_t> px(static_cast<size_t>(w) * static_cast<size_t>(h) *
                               4);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      bool on = ((x / cell) + (y / cell)) % 2 == 0;
      std::uint8_t c = on ? 240 : 30;
      size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) * 4 +
                   static_cast<size_t>(x) * 4;
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
