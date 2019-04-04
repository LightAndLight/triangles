#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <stdexcept>

class GLFW {
public:
  GLFW() {
    if (GLFW_FALSE == glfwInit()) {
      throw std::runtime_error("failed to initialize glfw");
    }

    if (GLFW_FALSE == glfwVulkanSupported()) {
      throw std::runtime_error("vulkan not supported");
    }
  }

  std::vector<const char*> getRequiredInstanceExtensions() {
    uint32_t count;
    const char **res = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char*> vec(res, res + count);
    return vec;
  }

  ~GLFW() {
    glfwTerminate();
  }
};

class Window {
private:
  GLFWwindow *window;
public:
  Window(int w, int h, const char *title) {
    this->window = glfwCreateWindow(w, h, title, nullptr, nullptr);
  }

  ~Window() {
    glfwDestroyWindow(this->window);
  }
};

int main() {
  GLFW context = GLFW();

  std::vector<const char*> layers =
    { "VK_LAYER_LUNARG_standard_validation"
    };
  std::vector<const char*> requiredExts = context.getRequiredInstanceExtensions();

  vk::ApplicationInfo appInfo
    ("triangle",
     VK_MAKE_VERSION(1,0,0),
     "No engine",
     VK_MAKE_VERSION(1,0,0),
     VK_MAKE_VERSION(1,0,82)
     );

  vk::InstanceCreateInfo instanceInfo
    ({},
     &appInfo,
     layers.size(), layers.data(),
     requiredExts.size(), requiredExts.data()
     );

  auto instance = vk::createInstanceUnique(instanceInfo);

  return 0;
}
