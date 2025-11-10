#pragma once
#include <string>
#include <vector>
#include "Mesh.hpp"

namespace gm {

class ObjLoader {
public:
    static Mesh LoadObjPNUV(const std::string& path);
};

}