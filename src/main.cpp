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
  GLFWwindow *window;
  uint32_t width;
  uint32_t height;
  const char *title;
  vk::Instance instance;
  vk::SurfaceKHR surface;
  vk::DispatchLoaderDynamic loader;
  vk::DebugUtilsMessengerEXT messenger;
  vk::PhysicalDevice physicalDevice;
  vk::Device device;
  uint32_t *graphicsQfIx;
  uint32_t *presentQfIx;
  vk::Queue graphicsQueue;
  vk::Queue presentQueue;
  vk::Format swapchainFormat;
  vk::Extent2D swapchainExtent;
  vk::SwapchainKHR swapchain;
  vk::RenderPass renderpass;
  std::vector<vk::ImageView> imageViews;
  std::vector<vk::Framebuffer> framebuffers;
  vk::CommandPool commandPool;
  std::vector<vk::CommandBuffer> commandBuffers;
  vk::PipelineLayout pipelineLayout;
  vk::Pipeline pipeline;
  vk::Semaphore imageAvailableSem;
  vk::Semaphore renderFinishedSem;


public:
  Context() { }

  ~Context() {
    if (this->graphicsQfIx) free(this->graphicsQfIx);
    if (this->presentQfIx) free(this->presentQfIx);

    if (this->device) {

      this->device.waitIdle();

      if (this->renderFinishedSem)
        this->device.destroySemaphore(this->renderFinishedSem);

      if (this->imageAvailableSem)
        this->device.destroySemaphore(this->imageAvailableSem);

      if (this->pipeline)
        this->device.destroyPipeline(this->pipeline);

      if (this->pipelineLayout)
        this->device.destroyPipelineLayout(this->pipelineLayout);

      if (this->commandPool) {

        if (!this->commandBuffers.empty())
          this->device.freeCommandBuffers(this->commandPool, this->commandBuffers);

        this->device.destroyCommandPool(this->commandPool);

      }

      for (auto &f : this->framebuffers) {
        this->device.destroyFramebuffer(f);
      }
      for (auto &i : this->imageViews) {
        this->device.destroyImageView(i);
      }

      if (this->renderpass)
        this->device.destroyRenderPass(this->renderpass);

      if (this->swapchain)
        this->device.destroySwapchainKHR(this->swapchain, nullptr, this->loader);
      this->device.destroy();

    }

    if (this->instance) {

      this->instance.destroySurfaceKHR(this->surface);
      if (this->messenger)
        this->instance.destroyDebugUtilsMessengerEXT(this->messenger, nullptr, this->loader);
      this->instance.destroy();

    }

    if (this->window) {

      glfwDestroyWindow(this->window);

    }

    glfwTerminate();
  }

  bool shouldClose() const {
    return glfwWindowShouldClose(this->window);
  }

  void drawFrame() const {
    assert(this->device);
    assert(this->imageAvailableSem);
    assert(this->swapchain);
    assert(!this->commandBuffers.empty());
    assert(this->graphicsQueue);
    assert(this->presentQueue);

    vk::ResultValue<uint32_t> o_ix =
      this->device.acquireNextImageKHR
        <vk::DispatchLoaderDynamic>
        (this->swapchain,
         std::numeric_limits<uint64_t>::max(),
         this->imageAvailableSem,
         vk::Fence(),
         this->loader);

    if (o_ix.result != vk::Result::eSuccess) {
      throw std::runtime_error(vk::to_string(o_ix.result));
    }

    uint32_t ix = o_ix.value;

    std::vector<vk::Semaphore> waitSemaphores =
      { imageAvailableSem };
    std::vector<vk::PipelineStageFlags> waitMasks =
      { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    std::vector<vk::CommandBuffer> submitBuffers =
      { this->commandBuffers[ix] };
    std::vector<vk::Semaphore> signalSemaphores =
      { renderFinishedSem };
    vk::SubmitInfo submitInfo
      (waitSemaphores.size(),
       waitSemaphores.data(),
       waitMasks.data(),
       submitBuffers.size(),
       submitBuffers.data(),
       signalSemaphores.size(),
       signalSemaphores.data());

    this->graphicsQueue.submit({ submitInfo }, nullptr);

    vk::PresentInfoKHR presentInfo
      (signalSemaphores.size(),
       signalSemaphores.data(),
       1,
       &(this->swapchain),
       &ix);

    this->presentQueue.presentKHR(presentInfo, this->loader);

  }

  void initWindow(uint32_t w, uint32_t h, const char *title) {
    if (GLFW_FALSE == glfwInit()) {
      throw std::runtime_error("failed to initialize glfw");
    }

    if (GLFW_FALSE == glfwVulkanSupported()) {
      throw std::runtime_error("vulkan not supported");
    }

    this->width = w;
    this->height = h;
    this->title = title;
    this->window = glfwCreateWindow(w, h, title, nullptr, nullptr);
  }

  void initInstance() {
    assert(this->title);
    assert(this->window);

    std::vector<const char*> layers =
      { "VK_LAYER_LUNARG_standard_validation"
      };

    uint32_t extCount;
    const char **_exts = glfwGetRequiredInstanceExtensions(&extCount);
    std::vector<const char*> requiredExts(_exts, _exts + extCount);

    requiredExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    vk::ApplicationInfo appInfo
      (this->title,
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

    this->instance = vk::createInstance(instanceInfo);

    this->loader = vk::DispatchLoaderDynamic(instance);
  }

  void initDebugMessenger() {
    assert(this->instance);

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

    this->messenger =
      instance.createDebugUtilsMessengerEXT(messengerInfo, nullptr, this->loader);
  }

  void getPhysicalDevice() {
    assert(this->instance);

    std::vector<vk::PhysicalDevice> devices = this->instance.enumeratePhysicalDevices();

    if (devices.empty()) {
      throw std::runtime_error("no physical devices");
    }

    this->physicalDevice = devices[0];
  }

  void getQueueIndices() {
    assert(this->physicalDevice);
    assert(this->surface);

    std::vector<vk::QueueFamilyProperties> queueFamilies =
      this->physicalDevice.getQueueFamilyProperties();

    std::vector<uint32_t> graphicsQfIxs, presentQfIxs;
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
      if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
        graphicsQfIxs.push_back(i);
      }

      if (this->physicalDevice.getSurfaceSupportKHR(i, this->surface, this->loader)) {
        presentQfIxs.push_back(i);
      }
    }

    if (graphicsQfIxs.empty()) {
      throw std::runtime_error("no graphics queues");
    }

    if (presentQfIxs.empty()) {
      throw std::runtime_error("no present queues");
    }

    this->graphicsQfIx = (uint32_t*) malloc(sizeof(uint32_t));
    *this->graphicsQfIx = graphicsQfIxs[0];

    this->presentQfIx = (uint32_t*) malloc(sizeof(uint32_t));
    *this->presentQfIx = presentQfIxs[0];
  }

  void initDevice() {
    assert(this->graphicsQfIx);
    assert(this->presentQfIx);
    assert(this->physicalDevice);

    std::unordered_set<uint32_t> ixs = { *this->graphicsQfIx, *this->presentQfIx };

    std::vector<vk::DeviceQueueCreateInfo> queueInfos(ixs.size());
    size_t ix = 0;
    for (auto it = ixs.begin(); it != ixs.end(); ++it) {
      const float priorities = 1.0;
      queueInfos[ix] = vk::DeviceQueueCreateInfo({}, *it, 1, &priorities);
      ++ix;
    }

    vk::PhysicalDeviceFeatures features = this->physicalDevice.getFeatures();
    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    vk::DeviceCreateInfo deviceInfo
      ({},
       queueInfos.size(), queueInfos.data(),
       0, nullptr,
       deviceExtensions.size(), deviceExtensions.data(),
       &features);
    this->device = this->physicalDevice.createDevice(deviceInfo, nullptr, this->loader);
  }

  void getSurface() {
    assert(this->window);
    assert(this->instance);
    VkSurfaceKHR _surface;
    if (VK_SUCCESS != glfwCreateWindowSurface(this->instance, this->window, NULL, &_surface)) {
      throw std::runtime_error("couldn't create surface");
    }
    this->surface = _surface;
  }

  void getQueues() {
    assert(this->graphicsQfIx);
    assert(this->presentQfIx);
    assert(this->device);
    this->graphicsQueue = this->device.getQueue(*this->graphicsQfIx, 0);
    this->presentQueue = this->device.getQueue(*this->presentQfIx, 0);
  }

  void initSwapchain() {
    assert(this->physicalDevice);
    assert(this->surface);
    assert(this->window);

    vk::SurfaceCapabilitiesKHR capabilities =
      this->physicalDevice.getSurfaceCapabilitiesKHR(this->surface, this->loader);
    uint32_t minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
      minImageCount = std::min(minImageCount, capabilities.maxImageCount);
    }

    std::vector<vk::SurfaceFormatKHR> formats =
      this->physicalDevice.getSurfaceFormatsKHR(this->surface, this->loader);

    this->swapchainFormat = formats[0].format;

    vk::ColorSpaceKHR swapchainColorSpace = formats[0].colorSpace;
    if (formats.empty()) {
      throw std::runtime_error("no swapchain formats");
    }
    for (auto &f : formats) {
      if (vk::Format::eUndefined == f.format ||
          (vk::Format::eB8G8R8A8Unorm == f.format &&
           vk::ColorSpaceKHR::eSrgbNonlinear == f.colorSpace)) {

        this->swapchainFormat = vk::Format::eB8G8R8A8Unorm;
        swapchainColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;

        break;
      }
    }

    this->swapchainExtent =
      vk::Extent2D
      (std::min
         (std::max(this->width, capabilities.minImageExtent.width),
          capabilities.maxImageExtent.width),
       std::min
         (std::max(this->height, capabilities.minImageExtent.height),
          capabilities.maxImageExtent.height));

    vk::SharingMode sharingMode;
    std::vector<uint32_t> sharingIndices;
    if (*this->graphicsQfIx == *this->presentQfIx) {
      sharingMode = vk::SharingMode::eExclusive;
      sharingIndices = std::vector<uint32_t>(0);
    } else {
      sharingMode = vk::SharingMode::eConcurrent;
      sharingIndices = { *this->graphicsQfIx, *this->presentQfIx };
    }

    std::vector<vk::PresentModeKHR> presentModes =
      this->physicalDevice.getSurfacePresentModesKHR(this->surface, this->loader);
    vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;
    for (auto &pm : presentModes) {
      if (vk::PresentModeKHR::eMailbox == pm) {
        swapchainPresentMode = pm;
        break;
      }
    }

    vk::SwapchainCreateInfoKHR swapchainInfo
      ({},
       surface,
       minImageCount,
       this->swapchainFormat,
       swapchainColorSpace,
       this->swapchainExtent,
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

    this->swapchain = this->device.createSwapchainKHR(swapchainInfo, nullptr, this->loader);
  }

  void initRenderPass() {
    assert(this->device);
    assert(this->swapchain);

    vk::AttachmentDescription colorAttachment
      ({},
       this->swapchainFormat,
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

    this->renderpass = this->device.createRenderPass(renderpassInfo);
  }

  void initImageViews() {
    assert(this->device);
    assert(this->swapchain);

    std::vector<vk::Image> images =
      this->device.getSwapchainImagesKHR(this->swapchain, this->loader);

    this->imageViews = std::vector<vk::ImageView>(images.size());

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
      this->imageViews[i] = this->device.createImageView(imageViewInfo);
    }
  }

  void initFramebuffers() {
    assert(this->device);
    assert(this->swapchain);
    assert(this->renderpass);
    assert(!this->imageViews.empty());

    size_t count = this->imageViews.size();
    this->framebuffers = std::vector<vk::Framebuffer>(count);
    for (size_t i = 0; i < count; ++i) {
      vk::FramebufferCreateInfo framebufferInfo
        ({},
         this->renderpass,
         1, &imageViews[i],
         this->swapchainExtent.width, this->swapchainExtent.height,
         1);

      this->framebuffers[i] = this->device.createFramebuffer(framebufferInfo);
    }
  }

  void initCommandPool() {
    vk::CommandPoolCreateInfo commandPoolInfo({}, *this->graphicsQfIx);
    this->commandPool = this->device.createCommandPool(commandPoolInfo);
  }

  void initCommandBuffers() {
    assert(this->device);
    assert(this->commandPool);
    assert(!this->framebuffers.empty());
    assert(this->renderpass);
    assert(this->swapchain);
    assert(this->pipeline);

    vk::CommandBufferAllocateInfo allocateInfo
      (this->commandPool,
       vk::CommandBufferLevel::ePrimary,
       this->framebuffers.size());

    this->commandBuffers =
      this->device.allocateCommandBuffers(allocateInfo);

    for (size_t i = 0; i < this->commandBuffers.size(); ++i) {
      auto &c = this->commandBuffers[i];
      vk::CommandBufferBeginInfo beginInfo
        (vk::CommandBufferUsageFlagBits::eSimultaneousUse, nullptr);
      c.begin(beginInfo);

      std::vector<vk::ClearValue> clearValues =
        { vk::ClearValue().setColor(vk::ClearColorValue().setFloat32({{ 0, 1, 0, 1 }}))
        };
      vk::RenderPassBeginInfo renderpassBeginInfo
        (this->renderpass,
          this->framebuffers[i],
          vk::Rect2D(vk::Offset2D(0, 0), this->swapchainExtent),
          clearValues.size(),
          clearValues.data()
          );
      c.beginRenderPass(renderpassBeginInfo, vk::SubpassContents::eInline);

      c.bindPipeline(vk::PipelineBindPoint::eGraphics, this->pipeline);
      c.draw(3, 1, 0, 0);

      c.endRenderPass();

      c.end();
    }
  }

  vk::ShaderModule loadShader(const char *path) {
    assert(this->device);

    std::ifstream shaderFile(path, std::ios_base::binary | std::ios_base::ate);
    if (!shaderFile.is_open()) {
      throw std::runtime_error("couldn't open shader file");
    }
    uint32_t shaderCodeSize = shaderFile.tellg();
    std::vector<char> shaderData(shaderCodeSize);
    shaderFile.seekg(0);
    shaderFile.read(shaderData.data(), shaderCodeSize);
    vk::ShaderModuleCreateInfo shaderInfo
      ({},
       shaderCodeSize,
       (uint32_t*) shaderData.data());
    return this->device.createShaderModule(shaderInfo);
  }

  void initPipeline() {
    assert(this->device);
    assert(this->swapchain);

    vk::ShaderModule vertexShaderModule = this->loadShader("shaders/vert.spv");
    vk::ShaderModule fragmentShaderModule = this->loadShader("shaders/frag.spv");

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

    vk::Viewport viewport =
      vk::Viewport(0, 0, this->swapchainExtent.width, this->swapchainExtent.height, 0, 1);
    vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), this->swapchainExtent);
    vk::PipelineViewportStateCreateInfo viewportInfo
      ({},
       1, &viewport,
       1, &scissor
       );

    vk::PipelineRasterizationStateCreateInfo rasterizationInfo
      ({},
       false,
       false,
       vk::PolygonMode::eFill,
       vk::CullModeFlagBits::eBack,
       vk::FrontFace::eClockwise,
       false,
       0,
       0,
       0,
       1);

    vk::PipelineMultisampleStateCreateInfo multisampleInfo
      ({},
       vk::SampleCountFlagBits::e1,
       false,
       0,
       nullptr,
       false,
       false);

    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments =
      { vk::PipelineColorBlendAttachmentState
          (false,
           vk::BlendFactor::eOne,
           vk::BlendFactor::eZero,
           vk::BlendOp::eAdd,
           vk::BlendFactor::eOne,
           vk::BlendFactor::eZero,
           vk::BlendOp::eAdd,
           vk::ColorComponentFlagBits::eR |
           vk::ColorComponentFlagBits::eG |
           vk::ColorComponentFlagBits::eB |
           vk::ColorComponentFlagBits::eA
           )
      };
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo
      ({},
       false,
       vk::LogicOp::eNoOp,
       colorBlendAttachments.size(),
       colorBlendAttachments.data(),
       {{ 0, 0, 0, 0 }}
       );

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo ({}, {}, {});
    this->pipelineLayout = this->device.createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo graphicsPipelineInfo
      ({},
       shaderStageInfos.size(),
       shaderStageInfos.data(),
       &vertexInputInfo,
       &inputAssemblyInfo,
       nullptr,
       &viewportInfo,
       &rasterizationInfo,
       &multisampleInfo,
       nullptr,
       &colorBlendInfo,
       nullptr,
       this->pipelineLayout,
       this->renderpass,
       0,
       nullptr,
       -1);
    this->pipeline =
      this->device.createGraphicsPipeline(nullptr, graphicsPipelineInfo);

    this->device.destroyShaderModule(vertexShaderModule);
    this->device.destroyShaderModule(fragmentShaderModule);
  }

  void initSemaphores() {
    this->imageAvailableSem = device.createSemaphore(vk::SemaphoreCreateInfo());
    this->renderFinishedSem = device.createSemaphore(vk::SemaphoreCreateInfo());
  }
};

int main() {
  Context context = Context();

  context.initWindow(1280, 960, "triangle");
  context.initInstance();
  context.initDebugMessenger();
  context.getPhysicalDevice();
  context.getSurface();
  context.getQueueIndices();
  context.initDevice();
  context.getQueues();
  context.initSwapchain();
  context.initRenderPass();
  context.initImageViews();
  context.initFramebuffers();
  context.initPipeline();
  context.initCommandPool();
  context.initCommandBuffers();
  context.initSemaphores();

  while (!context.shouldClose()) {
    glfwPollEvents();
    context.drawFrame();
  }

  return 0;
}
