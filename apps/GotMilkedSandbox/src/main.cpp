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
#include "Shader.hpp"
#include "Texture.hpp"
#include "Transform.hpp"

// --- helpers ---
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

// Build a P/N/UV quad centered at origin in XY-plane (Z=0), front-facing +Z
static void createCowQuad(GLuint &vao, GLuint &vbo, GLuint &ebo) {
  // interleaved: Px Py Pz | Nx Ny Nz | U V   (8 floats per vertex)
  const float n[3] = {0.0f, 0.0f, 1.0f};
  std::vector<float> data = {// left-bottom
                             -0.5f, -0.5f, 0.0f, n[0], n[1], n[2], 0.0f, 0.0f,
                             0.5f,  -0.5f, 0.0f, n[0], n[1], n[2], 1.0f, 0.0f,
                             0.5f,  0.5f,  0.0f, n[0], n[1], n[2], 1.0f, 1.0f,
                             -0.5f, 0.5f,  0.0f, n[0], n[1], n[2], 0.0f, 1.0f};
  std::vector<unsigned int> idx = {0, 1, 2, 2, 3, 0};

  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int),
               idx.data(), GL_STATIC_DRAW);

  const GLsizei stride = 8 * sizeof(float);
  // layout(location = 0) vec3 aPos
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
  // layout(location = 1) vec3 aNormal
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));
  // layout(location = 2) vec2 aUV
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                        (void *)(6 * sizeof(float)));

  glBindVertexArray(0);
}

int main() {
  // --- init ---
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

  glfwSetScrollCallback(window, [](GLFWwindow *, double, double yoff) {
    float &fov = FovState::ref();
    fov -= static_cast<float>(yoff) * 2.0f;
    if (fov < 30.0f)
      fov = 30.0f;
    if (fov > 100.0f)
      fov = 100.0f;
  });

  glEnable(GL_DEPTH_TEST);

  // --- shader ---
  const std::string shaderDir = std::string(GM_ASSETS_DIR) + "/shaders";
  Shader shader;
  if (!shader.loadFromFiles(shaderDir + "/simple.vert.glsl",
                            shaderDir + "/simple.frag.glsl")) {
    std::fprintf(stderr, "%s Shader setup failed\n", NAME);
    return 1;
  }

  // --- cow texture (Variant B: abort on error) ---
  const std::string cowPath = std::string(GM_ASSETS_DIR) + "/textures/cow.png";
  Texture cow = Texture::loadOrDie(cowPath); // <-- Variante B

  shader.use();
  shader.setInt("uTex", 0); // sampler binding (we use texture unit 0)

  // --- cow quad geometry (P/N/UV) ---
  GLuint cowVAO = 0, cowVBO = 0, cowEBO = 0;
  createCowQuad(cowVAO, cowVBO, cowEBO);

  // --- camera & controls ---
  Camera cam; // (0,0,2)
  float mouseSensitivity = 0.12f;
  bool mouseCaptured = false, firstCapture = true;
  bool wireframe = false;
  double lastMouseX = 0.0, lastMouseY = 0.0;

  // --- timing/fps ---
  double lastTime = glfwGetTime();
  double lastTitle = lastTime;
  int frames = 0;

  // --- lighting constants ---
  glm::vec3 lightDir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
  glm::vec3 lightColor = glm::vec3(1.0f);

  while (!glfwWindowShouldClose(window)) {
    const double now = glfwGetTime();
    const float dt = static_cast<float>(now - lastTime);
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
      const double dx = mx - lastMouseX;
      const double dy = lastMouseY - my; // invert Y
      lastMouseX = mx;
      lastMouseY = my;
      cam.addYawPitch(static_cast<float>(dx) * mouseSensitivity,
                      static_cast<float>(dy) * mouseSensitivity);
    }

    // movement (+ Shift boost)
    const float baseSpeed = 3.0f;
    const float speedMul =
        (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 4.0f : 1.0f;
    const float step = baseSpeed * speedMul * dt;
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
    const float fov = FovState::ref();
    const glm::mat4 proj =
        glm::perspective(glm::radians(fov), aspect, 0.1f, 100.0f);
    const glm::mat4 view = cam.view();

    // model for cow: spin a bit so man sieht's
    Transform T;
    T.rotationDeg.z = static_cast<float>(now) * 20.0f;
    const glm::mat4 model = T.toMat4();
    const glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));

    shader.use();
    // basic matrices
    shader.setMat4("uModel", model);
    shader.setMat4("uView", view);
    shader.setMat4("uProj", proj);
    // normal matrix
    {
      const GLint loc = shader.uniformLoc("uNormalMat");
      if (loc >= 0)
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(normalMat));
    }
    // lighting uniforms
    {
      const GLint lp = shader.uniformLoc("uViewPos");
      if (lp >= 0)
        glUniform3fv(lp, 1, glm::value_ptr(cam.position()));
      const GLint ld = shader.uniformLoc("uLightDir");
      if (ld >= 0)
        glUniform3fv(ld, 1, glm::value_ptr(lightDir));
      const GLint lc = shader.uniformLoc("uLightColor");
      if (lc >= 0)
        glUniform3fv(lc, 1, glm::value_ptr(lightColor));
    }

    // textured draw
    const GLint uUseTexLoc = shader.uniformLoc("uUseTex");
    if (uUseTexLoc >= 0)
      glUniform1i(uUseTexLoc, 1); // use texture
    cow.bind(0);                  // bind to unit 0 (uTex=0)

    glBindVertexArray(cowVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // FPS title
    frames++;
    if (now - lastTitle >= 0.5) {
      double fps = frames / (now - lastTitle);
      lastTitle = now;
      frames = 0;
      char title[160];
      std::snprintf(title, sizeof(title),
                    "GotMilked  |  FPS: %.1f  |  VSync: %s  |  Wireframe: %s  "
                    "|  FOV: %.1f",
                    fps, boolStr(vsyncOn), boolStr(wireframe), fov);
      glfwSetWindowTitle(window, title);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // cleanup
  glDeleteBuffers(1, &cowEBO);
  glDeleteBuffers(1, &cowVBO);
  glDeleteVertexArrays(1, &cowVAO);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
