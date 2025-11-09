#pragma once
#include <cstdint>
#include <glad/glad.h>
#include <string>
#include <vector>

class Texture {
public:
  Texture() = default;
  ~Texture();

  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;
  Texture(Texture &&o) noexcept;
  Texture &operator=(Texture &&o) noexcept;

  // Create from raw RGBA8 pixels
  bool createRGBA8(int width, int height,
                   const std::vector<std::uint8_t> &pixels,
                   bool generateMipmaps = true);

  // Load PNG/JPG/etc. from disk (returns false on failure)
  bool load(const std::string &path, bool flipY = true,
            bool generateMipmaps = true);

  // Bind to texture unit
  void bind(int unit = 0) const;

  GLuint id() const { return m_id; }

  // Helper: procedural checkerboard
  static Texture makeChecker(int w = 128, int h = 128, int cell = 16);

private:
  GLuint m_id{0};
};
