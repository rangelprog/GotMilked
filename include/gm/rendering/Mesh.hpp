#pragma once
#include <vector>
#include <glad/glad.h>

namespace gm {

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    static Mesh fromPositions(const std::vector<float>& positions);
    static Mesh fromIndexed(const std::vector<float>& vertexData,
                          const std::vector<unsigned int>& indices,
                          int componentsPerVertex = 0);

    void Draw() const;
    void DrawInstanced(unsigned int instanceCount) const;

private:
    GLuint VAO{0};
    GLuint VBO{0};
    GLuint EBO{0};
    int vertexCount{0};
    bool hasIndices{false};
};

}