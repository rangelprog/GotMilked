#pragma once
#include <string>
#include <vector>
#include "gm/rendering/Mesh.hpp"

namespace gm {

class ObjLoader {
public:
    static Mesh LoadObjPNUV(const std::string& path);
};

}