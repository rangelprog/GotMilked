#include <cstdio>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Transform.hpp"

static void setVSync(bool on) { glfwSwapInterval(on ? 1 : 0); }
static const char *boolStr(bool v) { return v ? "ON" : "OFF"; }

struct FovState {
  static float &ref() {
    static float f = 60.0f;
    return f;
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
  if (!glfwInit())
    return 1;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "GotMilked", nullptr, nullptr);
  if (!window)
    return 1;
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    return 1;

  bool vsyncOn = true;
  setVSync(vsyncOn);
  glEnable(GL_DEPTH_TEST);

  glfwSetScrollCallback(window, [](GLFWwindow *, double, double yoff) {
    float &fov = FovState::ref();
    fov -= (float)yoff * 2.0f;
    if (fov < 30)
      fov = 30;
    if (fov > 100)
      fov = 100;
  });

  // Shader
  const std::string shaderDir = std::string(GM_ASSETS_DIR) + "/shaders";
  Shader shader;
  shader.loadFromFiles(shaderDir + "/simple.vert.glsl",
                       shaderDir + "/simple.frag.glsl");
  shader.use();
  shader.setInt("uTex", 0);

  // Cow Texture
  Texture cow =
      Texture::loadOrDie(std::string(GM_ASSETS_DIR) + "/textures/cow.png");

  // Mesh (Quad mit P/N/UV)
  Mesh cowMesh = Mesh::fromPNUV_Quad();

  // Scene Setup
  Scene scene;
  SceneEntity cowEntity;
  cowEntity.mesh = &cowMesh;
  cowEntity.texture = &cow;
  cowEntity.transform.position = {0.f, 0.f, 0.f};
  scene.add(cowEntity);

  // Camera
  Camera cam;
  float mouseSensitivity = 0.12f;
  bool mouseCaptured = false, firstCapture = true;
  double lastX = 0, lastY = 0;

  double lastTime = glfwGetTime();
  double lastTitle = lastTime;
  int frames = 0;

  while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    float dt = (float)(now - lastTime);
    lastTime = now;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, 1);

    // RMB capture toggle
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
        !mouseCaptured) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      mouseCaptured = true;
      firstCapture = true;
    } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) ==
                   GLFW_RELEASE &&
               mouseCaptured) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      mouseCaptured = false;
    }

    // Mouse Look
    if (mouseCaptured) {
      double mx, my;
      glfwGetCursorPos(window, &mx, &my);
      if (firstCapture) {
        lastX = mx;
        lastY = my;
        firstCapture = false;
      }
      cam.addYawPitch(float(mx - lastX) * mouseSensitivity,
                      float(lastY - my) * mouseSensitivity);
      lastX = mx;
      lastY = my;
    }

    float speed =
        dt *
        ((glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 6.f : 3.f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      cam.moveForward(speed);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      cam.moveBackward(speed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      cam.moveLeft(speed);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      cam.moveRight(speed);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
      cam.moveUp(speed);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
      cam.moveDown(speed);

    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.10f, 0.10f, 0.12f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw Scene
    scene.draw(shader, cam, fbw, fbh, FovState::ref());

    // FPS title
    frames++;
    if (now - lastTitle >= 0.5) {
      double fps = frames / (now - lastTitle);
      lastTitle = now;
      frames = 0;
      char title[128];
      sprintf_s(title, "GotMilked | FPS: %.1f | FOV: %.1f", fps,
                FovState::ref());
      glfwSetWindowTitle(window, title);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
