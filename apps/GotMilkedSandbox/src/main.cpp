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
#include "ObjLoader.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "Transform.hpp"

// -----------------------------------------------------
static void setVSync(bool on) { glfwSwapInterval(on ? 1 : 0); }
static const char *boolStr(bool v) { return v ? "ON" : "OFF"; }

struct FovState {
  static float &ref() {
    static float fov = 60.0f;
    return fov;
  }
};

static void error_callback(int code, const char *desc) {
  std::fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

#ifndef GM_ASSETS_DIR
#error GM_ASSETS_DIR must be defined (via CMake)
#endif
// -----------------------------------------------------

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
  glEnable(GL_DEPTH_TEST);

  bool vsyncOn = true;
  setVSync(vsyncOn);

  glfwSetScrollCallback(window, [](GLFWwindow *, double, double yoff) {
    float &f = FovState::ref();
    f -= (float)yoff * 2.0f;
    if (f < 30.0f)
      f = 30.0f;
    if (f > 100.0f)
      f = 100.0f;
  });

  // --- Shader ---
  const std::string shaderDir = std::string(GM_ASSETS_DIR) + "/shaders";
  Shader shader;
  if (!shader.loadFromFiles(shaderDir + "/simple.vert.glsl",
                            shaderDir + "/simple.frag.glsl")) {
    return 1;
  }

  // --- Texture ---
  const std::string cowTexPath =
      std::string(GM_ASSETS_DIR) + "/textures/cow.png";
  Texture cowTex = Texture::loadOrDie(cowTexPath, true);
  shader.use();
  shader.setInt("uTex", 0); // sampler -> unit 0

  // --- Load Model ---
  const std::string cowObjPath = std::string(GM_ASSETS_DIR) + "/models/cow.obj";
  Mesh cowMesh = ObjLoader::loadObjPNUV(cowObjPath);

  // --- Camera / Controls ---
  Camera cam;
  float mouseSensitivity = 0.12f;
  bool mouseCaptured = false, firstCapture = true;
  double lastMouseX = 0.0, lastMouseY = 0.0;
  bool wireframe = false;

  // --- Timing ---
  double lastTime = glfwGetTime();
  double lastTitle = lastTime;
  int frames = 0;

  // --- Lighting ---
  glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
  glm::vec3 lightColor = glm::vec3(1.0f);

  while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    float dt = (float)(now - lastTime);
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

    // Mouse Look
    if (mouseCaptured) {
      double mx, my;
      glfwGetCursorPos(window, &mx, &my);
      if (firstCapture) {
        lastMouseX = mx;
        lastMouseY = my;
        firstCapture = false;
      }
      double dx = mx - lastMouseX;
      double dy = lastMouseY - my;
      lastMouseX = mx;
      lastMouseY = my;
      cam.addYawPitch((float)dx * mouseSensitivity,
                      (float)dy * mouseSensitivity);
    }

    // Movement
    const float baseSpeed = 3.0f;
    const float speed =
        baseSpeed *
        ((glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 4.0f
                                                                 : 1.0f) *
        dt;
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

    // Toggles
    {
      static bool pf = false, pv = false;
      bool fNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
      bool vNow = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
      if (fNow && !pf) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
      }
      if (vNow && !pv) {
        vsyncOn = !vsyncOn;
        setVSync(vsyncOn);
      }
      pf = fNow;
      pv = vNow;
    }

    // --- Render ---
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (float)fbw / (float)fbh;
    float fov = FovState::ref();
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 0.1f, 200.0f);
    glm::mat4 view = cam.view();

    Transform T;
    T.scale = glm::vec3(1.0f);
    T.rotationDeg.y = (float)now * 20.0f;
    glm::mat4 model = T.toMat4();
    glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

    shader.use();
    shader.setMat4("uModel", model);
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);

    if (GLint loc = shader.uniformLoc("uNormalMat"); loc >= 0)
      glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(normalMat));

    if (GLint loc = shader.uniformLoc("uViewPos"); loc >= 0)
      glUniform3fv(loc, 1, glm::value_ptr(cam.position()));
    if (GLint loc = shader.uniformLoc("uLightDir"); loc >= 0)
      glUniform3fv(loc, 1, glm::value_ptr(lightDir));
    if (GLint loc = shader.uniformLoc("uLightColor"); loc >= 0)
      glUniform3fv(loc, 1, glm::value_ptr(lightColor));

    if (GLint loc = shader.uniformLoc("uUseTex"); loc >= 0)
      glUniform1i(loc, 1);
    cowTex.bind(0);

    cowMesh.draw();

    // FPS title
    frames++;
    if (now - lastTitle >= 0.5) {
      double fps = frames / (now - lastTitle);
      lastTitle = now;
      frames = 0;
      char buf[160];
      std::snprintf(
          buf, sizeof(buf),
          "GotMilked | FPS: %.1f | VSync: %s | Wireframe: %s | FOV: %.1f", fps,
          boolStr(vsyncOn), boolStr(wireframe), fov);
      glfwSetWindowTitle(window, buf);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
