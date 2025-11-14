#include "gm/utils/ObjLoader.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/core/Error.hpp"

#include <glm/glm.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gm {

Mesh ObjLoader::LoadObjPNUV(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw core::ResourceError("mesh", path, "Unable to open OBJ file");
    }

    std::vector<glm::vec3> positions; // v
    std::vector<glm::vec3> normals;   // vn
    std::vector<glm::vec2> uvs;       // vt

    struct PTN {
      int p, t, n;
    };
    std::vector<PTN> faceTriples; // ungepackte Face-Daten

    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind("v ", 0) == 0) {
        std::istringstream ss(line.substr(2));
        glm::vec3 p;
        ss >> p.x >> p.y >> p.z;
        positions.push_back(p);
      } else if (line.rfind("vt ", 0) == 0) {
        std::istringstream ss(line.substr(3));
        glm::vec2 uv;
        ss >> uv.x >> uv.y;
        uvs.push_back(uv);
      } else if (line.rfind("vn ", 0) == 0) {
        std::istringstream ss(line.substr(3));
        glm::vec3 n;
        ss >> n.x >> n.y >> n.z;
        normals.push_back(n);
      } else if (line.rfind("f ", 0) == 0) {
        std::istringstream ss(line.substr(2));
        std::string vtx;
        std::vector<PTN> tmp;

        while (ss >> vtx) {
          auto parseIndex = [](std::string_view token) -> int {
            if (token.empty()) {
              return -1;
            }
            try {
              const int value = std::stoi(std::string(token));
              return value > 0 ? value - 1 : value;
            } catch (...) {
              return -1;
            }
          };

          PTN triple{-1, -1, -1};
          const size_t firstSlash = vtx.find('/');
          if (firstSlash == std::string::npos) {
            triple.p = parseIndex(vtx);
          } else {
            triple.p = parseIndex(std::string_view(vtx).substr(0, firstSlash));
            const size_t secondSlash = vtx.find('/', firstSlash + 1);
            if (secondSlash == std::string::npos) {
              // Format p/t
              triple.t = parseIndex(std::string_view(vtx).substr(firstSlash + 1));
            } else {
              // Format p/t/n or p//n
              const auto tToken = std::string_view(vtx).substr(firstSlash + 1, secondSlash - firstSlash - 1);
              if (!tToken.empty()) {
                triple.t = parseIndex(tToken);
              }
              triple.n = parseIndex(std::string_view(vtx).substr(secondSlash + 1));
            }
          }

          tmp.push_back(triple);
        }

        // Triangulieren falls mehr als 3
        for (size_t i = 1; i + 1 < tmp.size(); i++) {
          faceTriples.push_back(tmp[0]);
          faceTriples.push_back(tmp[i]);
          faceTriples.push_back(tmp[i + 1]);
        }
      }
    }

    // ------------------------------------------------------------
    // Duplicate-elimination + Interleaving
    // ------------------------------------------------------------
    struct Key {
      int p, t, n;
    };
    struct KeyHash {
      size_t operator()(const Key &k) const noexcept {
        return ((uint64_t)(k.p + 1) * 73856093ull) ^
               ((uint64_t)(k.t + 1) * 19349663ull) ^
               ((uint64_t)(k.n + 1) * 83492791ull);
      }
    };
    struct KeyEq {
      bool operator()(const Key &a, const Key &b) const noexcept {
        return a.p == b.p && a.t == b.t && a.n == b.n;
      }
    };

    std::unordered_map<Key, unsigned, KeyHash, KeyEq> dedup;
    std::vector<float> pnuv; // 8 floats: Px Py Pz Nx Ny Nz U V
    std::vector<unsigned> indices;

    auto emit = [&](int pi, int ti, int ni) -> unsigned {
      Key k{pi, ti, ni};
      auto it = dedup.find(k);
      if (it != dedup.end())
        return it->second;

      const glm::vec3 &P = positions.at((size_t)pi);
      const glm::vec3 N = (ni >= 0) ? normals.at((size_t)ni) : glm::vec3(0, 0, 1);
      const glm::vec2 UV = (ti >= 0) ? uvs.at((size_t)ti) : glm::vec2(0, 0);

      unsigned idx = (unsigned)(pnuv.size() / 8);
      pnuv.insert(pnuv.end(), {P.x, P.y, P.z, N.x, N.y, N.z, UV.x, UV.y});
      dedup.emplace(k, idx);
      return idx;
    };

    for (size_t i = 0; i < faceTriples.size(); i += 3) {
      unsigned a =
          emit(faceTriples[i + 0].p, faceTriples[i + 0].t, faceTriples[i + 0].n);
      unsigned b =
          emit(faceTriples[i + 1].p, faceTriples[i + 1].t, faceTriples[i + 1].n);
      unsigned c =
          emit(faceTriples[i + 2].p, faceTriples[i + 2].t, faceTriples[i + 2].n);
      indices.push_back(a);
      indices.push_back(b);
      indices.push_back(c);
    }

    return Mesh::fromIndexed(pnuv, indices);
}

} // namespace gm
