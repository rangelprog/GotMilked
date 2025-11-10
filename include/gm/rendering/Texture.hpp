#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>

namespace gm {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& o) noexcept;
    Texture& operator=(Texture&& o) noexcept;

    bool createRGBA8(int width, int height,
                    const std::vector<std::uint8_t>& pixels,
                    bool generateMipmaps = true);

    void bind(int unit = 0) const;
    GLuint id() const { return m_id; }

    static Texture makeChecker(int w = 256, int h = 256, int cell = 16);
    static Texture loadOrDie(const std::string& path, bool flipY = true);

private:
    GLuint m_id{0};
};

}