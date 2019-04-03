#include <vulkan/vulkan.hpp>

int main() {
  vk::ApplicationInfo appInfo
    ("triangle",
     VK_MAKE_VERSION(1,0,0),
     "No engine",
     VK_MAKE_VERSION(1,0,0),
     VK_MAKE_VERSION(1,0,82)
     );

    vk::InstanceCreateInfo instanceInfo({}, &appInfo, 0, nullptr, 0, nullptr);
  return 0;
}
