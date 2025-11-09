#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera.hpp"
#include "Mesh.hpp"
#include "Shader.hpp"
#include "Transform.hpp"

// helpers
static void setVSync(bool on) { glfwSwapInterval(on ? 1 : 0); }
static const char *boolStr(bool v) { return v ? "ON" : "OFF"; }

// FOV state for scroll callback
struct FovState {
  static float &ref() {
    static float fov = 60.0f;
    return fov;
  }
};

const char *NAME{"GotMilked:"};

#ifndef GM_ASSETS_DIR
#error GM_ASSETS_DIR must be defined (see CMakeLists.txt)
#endif

static void error_callback(int code, const char *desc) {
  std::fprintf(stderr, "%s GLFW error %d: %s\n", NAME, code, desc);
}

int main() {
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    std::fprintf(stderr, "%s GLFW init failed\n", NAME);
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "GotMilked", nullptr, nullptr);
  if (!window) {
    std::fprintf(stderr, "%s Window creation failed\n", NAME);
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::fprintf(stderr, "%s Failed to load OpenGL with glad\n", NAME);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  bool vsyncOn = true;
  setVSync(vsyncOn);

  // Scroll -> FOV (30..100°)
  glfwSetScrollCallback(window, [](GLFWwindow *, double, double yoff) {
    float &fov = FovState::ref();
    fov -= static_cast<float>(yoff) * 2.0f;
    if (fov < 30.0f)
      fov = 30.0f;
    if (fov > 100.0f)
      fov = 100.0f;
  });

  glEnable(GL_DEPTH_TEST);

  // geometry
  std::vector<float> triVerts = {-0.5f, -0.5f, 0.0f, 0.5f, -0.5f,
                                 0.0f,  0.0f,  0.5f, 0.0f};
  Mesh tri = Mesh::fromPositions(triVerts);

  std::vector<float> quadVerts = {
      -0.5f, -0.5f, 0.0f, // 0
      0.5f,  -0.5f, 0.0f, // 1
      0.5f,  0.5f,  0.0f, // 2
      -0.5f, 0.5f,  0.0f  // 3
  };
  std::vector<unsigned int> quadIdx = {0, 1, 2, 2, 3, 0};
  Mesh quad = Mesh::fromIndexed(quadVerts, quadIdx);

  // shader
  const std::string shaderDir = std::string(GM_ASSETS_DIR) + "/shaders";
  Shader shader;
  if (!shader.loadFromFiles(shaderDir + "/simple.vert.glsl",
                            shaderDir + "/simple.frag.glsl")) {
    std::fprintf(stderr, "%s Shader setup failed\n", NAME);
    return 1;
  }

  // camera & input
  Camera cam; // (0,0,2), yaw=-90, pitch=0
  float mouseSensitivity = 0.12f;
  bool mouseCaptured = false, firstCapture = true;
  double lastMouseX = 0.0, lastMouseY = 0.0;
  bool wireframe = false;

  // timing / fps
  double lastTime = glfwGetTime();
  double lastTitle = lastTime;
  int frames = 0;

  while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    float dt = static_cast<float>(now - lastTime);
    lastTime = now;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, 1);

    // RMB capture toggle
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
      if (!mouseCaptured) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        mouseCaptured = true;
        firstCapture = true;
      }
    } else if (mouseCaptured) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      mouseCaptured = false;
    }

    // mouse look
    if (mouseCaptured) {
      double mx, my;
      glfwGetCursorPos(window, &mx, &my);
      if (firstCapture) {
        lastMouseX = mx;
        lastMouseY = my;
        firstCapture = false;
      }
      double dx = mx - lastMouseX;
      double dy = lastMouseY - my; // invert Y
      lastMouseX = mx;
      lastMouseY = my;
      cam.addYawPitch(static_cast<float>(dx) * mouseSensitivity,
                      static_cast<float>(dy) * mouseSensitivity);
    }

    // movement (+Shift boost)
    float baseSpeed = 3.0f;
    float speedMul =
        (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 4.0f : 1.0f;
    float step = baseSpeed * speedMul * dt;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      cam.moveForward(step);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      cam.moveBackward(step);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      cam.moveLeft(step);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      cam.moveRight(step);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
      cam.moveUp(step);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
      cam.moveDown(step);

    // toggles: F (wireframe), V (vsync)
    {
      static bool prevF = false, prevV = false;
      bool fNow = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
      bool vNow = (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS);
      if (fNow && !prevF) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
      }
      if (vNow && !prevV) {
        vsyncOn = !vsyncOn;
        setVSync(vsyncOn);
      }
      prevF = fNow;
      prevV = vNow;
    }

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    if (fbw == 0 || fbh == 0) {
      glfwPollEvents();
      continue;
    }
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // matrices
    const float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
    const float fovNow = FovState::ref();
    const glm::mat4 proj =
        glm::perspective(glm::radians(fovNow), aspect, 0.1f, 100.0f);
    const glm::mat4 view = cam.view();
    const glm::mat4 viewProj = proj * view;

    const float t = static_cast<float>(now);

    shader.use();

    // A) rotierendes Dreieck
    {
      Transform A;
      A.rotationDeg.z = t * 45.0f;
      shader.setMat4("uMVP", viewProj * A.toMat4());
      tri.draw();
    }

    // B) Quad rechts, kleiner, gegenläufig
    {
      Transform B;
      B.position = {1.2f, 0.0f, 0.0f};
      B.rotationDeg.z = -t * 60.0f;
      B.scale = {0.8f, 0.8f, 0.8f};
      shader.setMat4("uMVP", viewProj * B.toMat4());
      quad.draw();
    }

    // FPS im Fenstertitel
    frames++;
    if (now - lastTitle >= 0.5) {
      double fps = frames / (now - lastTitle);
      lastTitle = now;
      frames = 0;
      char title[160];
      std::snprintf(title, sizeof(title),
                    "GotMilked  |  FPS: %.1f  |  VSync: %s  |  Wireframe: %s  "
                    "|  FOV: %.1f",
                    fps, boolStr(vsyncOn), boolStr(wireframe), fovNow);
      glfwSetWindowTitle(window, title);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
