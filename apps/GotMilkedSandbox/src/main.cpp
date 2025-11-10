#include <cstdio>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "Game.hpp"

static void error_callback(int code, const char *desc) {
  std::fprintf(stderr, "GLFW error %d: %s\n", code, desc);
}

int main() {
  // --- GLFW / OpenGL ---
  glfwSetErrorCallback(error_callback);
  if (!glfwInit())
    return 1;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  GLFWwindow *window = glfwCreateWindow(1280, 720, "GotMilkedSandbox", nullptr, nullptr);
  if (!window)
    return 1;
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    return 1;
  glEnable(GL_DEPTH_TEST);

  const std::string assetsDir = std::string(GM_ASSETS_DIR);
  Game game(assetsDir);
  if (!game.Init(window)) {
    return 1;
  }

  double lastTime = glfwGetTime();
  double lastTitle = lastTime;
  int frames = 0;

  bool vsyncOn = true;
  glfwSwapInterval(vsyncOn ? 1 : 0);

  while (!glfwWindowShouldClose(window)) {
    double now = glfwGetTime();
    float dt = (float)(now - lastTime);
    lastTime = now;

    game.Update(dt);
    game.Render();

    // FPS title
    frames++;
    if (now - lastTitle >= 0.5) {
      double fps = frames / (now - lastTitle);
      lastTitle = now;
      frames = 0;
      char buf[160];
      std::snprintf(buf, sizeof(buf), "GotMilkedSandbox | FPS: %.1f", fps);
      glfwSetWindowTitle(window, buf);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  game.Shutdown();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
