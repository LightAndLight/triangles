#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_TYPESAFE_CONVERSION
#include <vulkan/vulkan.hpp>

#include <fstream>
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
  vk::SwapchainKHR swapchain;
  vk::RenderPass renderpass;
  std::vector<vk::ImageView> imageViews;
  std::vector<vk::Framebuffer> framebuffers;
  vk::CommandPool commandPool;
  std::vector<vk::CommandBuffer> commandBuffers;

  Context
    (GLFWwindow *window,
     vk::Instance instance,
     vk::DispatchLoaderDynamic loader,
     vk::DebugUtilsMessengerEXT messenger,
     vk::SurfaceKHR surface,
     vk::Device device,
     vk::SwapchainKHR swapchain,
     vk::RenderPass renderpass,
     std::vector<vk::ImageView> imageViews,
     std::vector<vk::Framebuffer> framebuffers,
     vk::CommandPool commandPool,
     std::vector<vk::CommandBuffer> commandBuffers
     ) {

    this->instance = instance;
    this->window = window;
    this->surface = surface;
    this->loader = loader;
    this->messenger = messenger;
    this->device = device;
    this->swapchain = swapchain;
    this->renderpass = renderpass;
    this->imageViews = imageViews;
    this->framebuffers = framebuffers;
    this->commandPool = commandPool;
    this->commandBuffers = commandBuffers;

  }
public:
  ~Context() {
    this->device.waitIdle();

    this->device.freeCommandBuffers(this->commandPool, this->commandBuffers);

    this->device.destroyCommandPool(this->commandPool);

    for (auto &f : this->framebuffers) {
      this->device.destroyFramebuffer(f);
    }
    for (auto &i : this->imageViews) {
      this->device.destroyImageView(i);
    }

    this->device.destroyRenderPass(this->renderpass);
    this->device.destroySwapchainKHR(this->swapchain, nullptr, this->loader);
    this->device.destroy();
    this->instance.destroySurfaceKHR(this->surface);
    this->instance.destroyDebugUtilsMessengerEXT(this->messenger, nullptr, this->loader);
    this->instance.destroy();

    glfwDestroyWindow(this->window);
    glfwTerminate();
  }

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
    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::DeviceCreateInfo deviceInfo
      ({},
       queueInfos.size(), queueInfos.data(),
       0, nullptr,
       deviceExtensions.size(), deviceExtensions.data(),
       &features);
    vk::Device device = physicalDevice.createDevice(deviceInfo);

    vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    uint32_t minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
      minImageCount = std::min(minImageCount, capabilities.maxImageCount);
    }

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

    vk::SharingMode sharingMode;
    std::vector<uint32_t> sharingIndices;
    if (graphicsQfIx == presentQfIx) {
      sharingMode = vk::SharingMode::eExclusive;
      sharingIndices = std::vector<uint32_t>(0);
    } else {
      sharingMode = vk::SharingMode::eConcurrent;
      sharingIndices = { graphicsQfIx, presentQfIx };
    }
    sharingIndices.data();

    std::vector<vk::PresentModeKHR> presentModes =
      physicalDevice.getSurfacePresentModesKHR(surface);
    vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;
    for (auto &pm : presentModes) {
      if (vk::PresentModeKHR::eMailbox == pm) {
        swapchainPresentMode = pm;
      }
    }

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
       capabilities.currentTransform,
       vk::CompositeAlphaFlagBitsKHR::eOpaque,
       swapchainPresentMode,
       true,
       nullptr);
    vk::SwapchainKHR swapchain = device.createSwapchainKHR(swapchainInfo, nullptr, loader);

    vk::AttachmentDescription colorAttachment
      ({},
       swapchainFormat,
       vk::SampleCountFlagBits::e1,
       vk::AttachmentLoadOp::eClear,
       vk::AttachmentStoreOp::eStore,
       vk::AttachmentLoadOp::eDontCare,
       vk::AttachmentStoreOp::eDontCare,
       vk::ImageLayout::eUndefined,
       vk::ImageLayout::ePresentSrcKHR);

    std::vector<vk::AttachmentReference> colorAttachments =
      { vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal)
      };

    vk::SubpassDescription subpass
      ({},
       vk::PipelineBindPoint::eGraphics,
       0, nullptr,
       colorAttachments.size(), colorAttachments.data(), nullptr,
       nullptr,
       0, nullptr);

    vk::SubpassDependency subpassDep
      (VK_SUBPASS_EXTERNAL,
       0,
       vk::PipelineStageFlagBits::eColorAttachmentOutput,
       vk::PipelineStageFlagBits::eColorAttachmentOutput,
       {},
       vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite,
       {});

    std::vector<vk::AttachmentDescription> attachmentDescs = { colorAttachment };
    std::vector<vk::SubpassDescription> subpasses = { subpass };
    std::vector<vk::SubpassDependency> subpassDeps = { subpassDep };
    vk::RenderPassCreateInfo renderpassInfo
      ({},
       attachmentDescs.size(), attachmentDescs.data(),
       subpasses.size(), subpasses.data(),
       subpassDeps.size(), subpassDeps.data());

    vk::RenderPass renderpass = device.createRenderPass(renderpassInfo);

    std::vector<vk::Image> images = device.getSwapchainImagesKHR(swapchain, loader);
    std::vector<vk::ImageView> imageViews(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
      vk::ImageViewCreateInfo imageViewInfo
        ({},
         images[i],
         vk::ImageViewType::e2D,
         swapchainFormat,
         vk::ComponentMapping
           (vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity),
         vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
         );
      imageViews[i] = device.createImageView(imageViewInfo);
    }

    std::vector<vk::Framebuffer> framebuffers(images.size());
    for (size_t i = 0; i < imageViews.size(); ++i) {
      vk::FramebufferCreateInfo framebufferInfo
        ({},
         renderpass,
         1, &imageViews[i],
         swapchainExtent.width, swapchainExtent.height,
         1);

      framebuffers[i] = device.createFramebuffer(framebufferInfo);
    }

    vk::CommandPoolCreateInfo commandPoolInfo({}, graphicsQfIx);
    vk::CommandPool commandPool = device.createCommandPool(commandPoolInfo);

    vk::CommandBufferAllocateInfo allocateInfo
      (commandPool,
       vk::CommandBufferLevel::ePrimary,
       framebuffers.size());
    std::vector<vk::CommandBuffer> commandBuffers = device.allocateCommandBuffers(allocateInfo);

    std::ifstream vertexShaderFile("shaders/vert.spv", std::ios_base::binary | std::ios_base::ate);
    if (!vertexShaderFile.is_open()) {
      throw std::runtime_error("couldn't open vertex shader file");
    }
    uint32_t vertexShaderCodeSize = vertexShaderFile.tellg();
    std::vector<char> vertexShaderData(vertexShaderCodeSize);
    vertexShaderFile.read(vertexShaderData.data(), vertexShaderCodeSize);
    vk::ShaderModuleCreateInfo vertexShaderInfo
      ({},
       vertexShaderCodeSize,
       (uint32_t*) vertexShaderData.data());
    vk::ShaderModule vertexShaderModule = device.createShaderModule(vertexShaderInfo);

    std::ifstream fragmentShaderFile
      ("shaders/frag.spv",
       std::ios_base::binary | std::ios_base::ate);
    if (!fragmentShaderFile.is_open()) {
      throw std::runtime_error("couldn't open fragment shader file");
    }
    uint32_t fragmentShaderCodeSize = fragmentShaderFile.tellg();
    std::vector<char> fragmentShaderData(fragmentShaderCodeSize);
    fragmentShaderFile.read(fragmentShaderData.data(), fragmentShaderCodeSize);
    vk::ShaderModuleCreateInfo fragmentShaderInfo
      ({},
       fragmentShaderCodeSize,
       (uint32_t*) fragmentShaderData.data());
    vk::ShaderModule fragmentShaderModule = device.createShaderModule(fragmentShaderInfo);

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStageInfos =
      {
       vk::PipelineShaderStageCreateInfo
         ({},
          vk::ShaderStageFlagBits::eVertex,
          vertexShaderModule,
          "main",
          nullptr),

       vk::PipelineShaderStageCreateInfo
            ({},
             vk::ShaderStageFlagBits::eFragment,
             fragmentShaderModule,
             "main",
             nullptr)

      };

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo({}, {}, {});

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo
      ({},
       vk::PrimitiveTopology::eTriangleList,
       false);

    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo
      ({},
       shaderStageInfos,
       vertexInputInfo,
       inputAssemblyInfo,
       nullptr,
       viewportInfo,
       rasterizationInfo,
       multisampleInfo,
       depthStencilInfo,
       colorBlendInfo,
       dynamicStateInfo,
       pipelineLayout,
       renderpass,
       VK_NULL_HANDLE,
       -1);
    vk::Pipeline graphicsPipeline =
      device.createGraphicsPipeline(nullptr, graphicsPipelineInfo);

    device.destroyShaderModule(vertexShaderModule);
    device.destroyShaderModule(fragmentShaderModule);

    Context context =
      Context
        (window,
         instance,
         loader,
         messenger,
         surface,
         device,
         swapchain,
         renderpass,
         imageViews,
         framebuffers,
         commandPool,
         commandBuffers);
    return context;
  }

  vk::Instance getInstance() {
    return this->instance;
  }
};

int main() {
  vk::Optional<Context> context = Context::create(1280, 960, "triangle");

  return 0;
}
