#pragma once

#if GM_DEBUG_TOOLS

#include <functional>

struct GLFWwindow;

namespace gm {
class Camera;
}

namespace gm::debug {

/**
 * @brief Interface implemented by components that support runtime terrain editing controls.
 */
class ITerrainEditing {
public:
    virtual ~ITerrainEditing() = default;

    virtual void SetCamera(gm::Camera* camera) = 0;
    virtual void SetWindow(GLFWwindow* window) = 0;
    virtual void SetFovProvider(std::function<float()> provider) = 0;
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


