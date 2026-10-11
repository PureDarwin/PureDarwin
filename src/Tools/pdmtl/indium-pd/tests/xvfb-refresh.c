// LD_PRELOAD for headless runs: Xvfb reports a 0 Hz mode, Indium's tests divide by it for frame time
#define _GNU_SOURCE
#include <dlfcn.h>
#include <GLFW/glfw3.h>

const GLFWvidmode *glfwGetVideoMode(GLFWmonitor *m) {
  static GLFWvidmode fixed;
  const GLFWvidmode *(*real)(GLFWmonitor *) = (const GLFWvidmode *(*)(GLFWmonitor *))dlsym(RTLD_NEXT, "glfwGetVideoMode");
  const GLFWvidmode *v = real(m);

  if (!v || v->refreshRate)
    return v;
  fixed = *v;
  fixed.refreshRate = 60;
  return &fixed;
}
