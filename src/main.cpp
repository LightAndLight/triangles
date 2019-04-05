#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <unordered_set>

VkBool32 messengerCallback
  (VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
   VkDebugUtilsMessageTypeFlagsEXT messageTypes,
   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
   void* pUserData) {

  std::cerr << "validation: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}

class Context {
private:
  vk::Instance instance;
  GLFWwindow *window;
  vk::SurfaceKHR surface;
  vk::DispatchLoaderDynamic loader;
  vk::DebugUtilsMessengerEXT messenger;
  vk::Device device;

  Context
    (GLFWwindow *window,
     vk::Instance instance,
     vk::DispatchLoaderDynamic loader,
     vk::DebugUtilsMessengerEXT messenger,
     vk::SurfaceKHR surface,
     vk::Device device
     ) {

    this->instance = instance;
    this->window = window;
    this->surface = surface;
    this->loader = loader;
    this->messenger = messenger;
    this->device = device;

  }
public:
  static vk::Optional<Context> create(uint32_t w, uint32_t h, const char *title) {
    if (GLFW_FALSE == glfwInit()) {
      throw std::runtime_error("failed to initialize glfw");
    }

    if (GLFW_FALSE == glfwVulkanSupported()) {
      throw std::runtime_error("vulkan not supported");
    }

    GLFWwindow *window = glfwCreateWindow(w, h, title, nullptr, nullptr);

    std::vector<const char*> layers =
      { "VK_LAYER_LUNARG_standard_validation"
      };

    uint32_t extCount;
    const char **_exts = glfwGetRequiredInstanceExtensions(&extCount);
    std::vector<const char*> requiredExts(_exts, _exts + extCount);

    requiredExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    vk::ApplicationInfo appInfo
      (title,
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

    vk::Instance instance = vk::createInstance(instanceInfo);
    vk::DispatchLoaderDynamic loader(instance);

    vk::DebugUtilsMessengerCreateInfoEXT messengerInfo
      ({},

       vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
       vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,

       vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,

       messengerCallback,
       nullptr
       );

    vk::DebugUtilsMessengerEXT messenger = //instance.createDebugUtilsMessengerEXT(messengerInfo);
      instance.createDebugUtilsMessengerEXT(messengerInfo, nullptr, loader);

    std::vector<vk::PhysicalDevice> devices = instance.enumeratePhysicalDevices();

    if (devices.empty()) {
      throw std::runtime_error("no physical devices");
    }

    vk::PhysicalDevice physicalDevice = devices[0];

    std::vector<vk::QueueFamilyProperties> queueFamilies =
      physicalDevice.getQueueFamilyProperties();

    VkSurfaceKHR _surface;
    if (VK_SUCCESS != glfwCreateWindowSurface(instance, window, NULL, &_surface)) {
      throw std::runtime_error("couldn't create surface");
    }
    vk::SurfaceKHR surface(_surface);

    std::vector<uint32_t> graphicsQfIxs, presentQfIxs;
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
      if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
        graphicsQfIxs.push_back(i);
      }

      if (physicalDevice.getSurfaceSupportKHR(i, surface)) {
        presentQfIxs.push_back(i);
      }
    }

    if (graphicsQfIxs.empty()) {
      throw std::runtime_error("no graphics queues");
    }

    if (presentQfIxs.empty()) {
      throw std::runtime_error("no present queues");
    }

    uint32_t graphicsQfIx = graphicsQfIxs[0];
    uint32_t presentQfIx = presentQfIxs[0];

    std::unordered_set<uint32_t> ixs = { graphicsQfIx, presentQfIx };

    std::vector<vk::DeviceQueueCreateInfo> queueInfos(ixs.size());
    for (size_t i = 0; i < ixs.size(); ++i) {
      const float priorities = 1.0;
      queueInfos[i] = vk::DeviceQueueCreateInfo({}, i, 1, &priorities);
    }

    vk::PhysicalDeviceFeatures features = physicalDevice.getFeatures();
    vk::DeviceCreateInfo deviceInfo
      ({},
       queueInfos.size(), queueInfos.data(),
       0, nullptr,
       0, nullptr,
       &features);
    vk::Device device = physicalDevice.createDevice(deviceInfo);

    vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    uint32_t minImageCount = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

    std::vector<vk::SurfaceFormatKHR> formats = physicalDevice.getSurfaceFormatsKHR(surface);
    vk::Format swapchainFormat = formats[0].format;
    vk::ColorSpaceKHR swapchainColorSpace = formats[0].colorSpace;
    if (formats.empty()) {
      throw std::runtime_error("no swapchain formats");
    }
    for (auto &f : formats) {
      if (vk::Format::eUndefined == f.format ||
          (vk::Format::eB8G8R8A8Unorm == f.format &&
           vk::ColorSpaceKHR::eSrgbNonlinear == f.colorSpace)) {

        swapchainFormat = vk::Format::eB8G8R8A8Unorm;
        swapchainColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

        break;
      }
    }

    vk::Extent2D swapchainExtent
      (std::max
         (std::min(w, capabilities.minImageExtent.width),
          capabilities.maxImageExtent.width),
       std::max
         (std::min(h, capabilities.minImageExtent.height),
          capabilities.maxImageExtent.height));

    vk::SwapchainCreateInfoKHR swapchainInfo
      ({},
       surface,
       minImageCount,
       swapchainFormat,
       swapchainColorSpace,
       swapchainExtent,
       1,
       vk::ImageUsageFlagBits::eColorAttachment,
       sharingMode,
       sharingIndices.size(),
       sharingIndices.data(),
       _,
       vk::CompositeAlphaFlagBitsKHR::eOpaque,
       swapchainPresentMode,
       true);

    Context context = Context(window, instance, loader, messenger, surface, device);
    return context;
  }

  vk::Instance getInstance() {
    return this->instance;
  }

  ~Context() {
    this->device.destroy();
    this->instance.destroySurfaceKHR(this->surface);
    this->instance.destroyDebugUtilsMessengerEXT(this->messenger, nullptr, this->loader);
    this->instance.destroy();
    glfwDestroyWindow(this->window);
    glfwTerminate();
  }
};

int main() {
  vk::Optional<Context> context = Context::create(1280, 960, "triangle");

  return 0;
}
