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
  vk::Queue graphicsQueue;
  vk::Queue presentQueue;
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

  Context
    (GLFWwindow *window,
     vk::Instance instance,
     vk::DispatchLoaderDynamic loader,
     vk::DebugUtilsMessengerEXT messenger,
     vk::SurfaceKHR surface,
     vk::Device device,
     vk::Queue graphicsQueue,
     vk::Queue presentQueue,
     vk::SwapchainKHR swapchain,
     vk::RenderPass renderpass,
     std::vector<vk::ImageView> imageViews,
     std::vector<vk::Framebuffer> framebuffers,
     vk::CommandPool commandPool,
     std::vector<vk::CommandBuffer> commandBuffers,
     vk::PipelineLayout pipelineLayout,
     vk::Pipeline pipeline,
     vk::Semaphore imageAvailableSem,
     vk::Semaphore renderFinishedSem
     ) {

    this->instance = instance;
    this->window = window;
    this->surface = surface;
    this->loader = loader;
    this->messenger = messenger;
    this->device = device;
    this->graphicsQueue = graphicsQueue;
    this->presentQueue = presentQueue;
    this->swapchain = swapchain;
    this->renderpass = renderpass;
    this->imageViews = imageViews;
    this->framebuffers = framebuffers;
    this->commandPool = commandPool;
    this->commandBuffers = commandBuffers;
    this->pipelineLayout = pipelineLayout;
    this->pipeline = pipeline;
    this->imageAvailableSem = imageAvailableSem;
    this->renderFinishedSem = renderFinishedSem;

  }
public:
  ~Context() {
    this->device.waitIdle();

    this->device.destroySemaphore(this->renderFinishedSem);
    this->device.destroySemaphore(this->imageAvailableSem);

    this->device.destroyPipeline(this->pipeline);
    this->device.destroyPipelineLayout(this->pipelineLayout);

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

  bool shouldClose() const {
    return glfwWindowShouldClose(this->window);
  }

  void drawFrame() const {
    vk::ResultValue<uint32_t> o_ix =
      this->device.acquireNextImageKHR
        (this->swapchain,
         std::numeric_limits<uint64_t>::max(),
         imageAvailableSem,
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
    vk::Device device = physicalDevice.createDevice(deviceInfo, nullptr, loader);

    vk::Queue graphicsQueue = device.getQueue(graphicsQfIx, 0);
    vk::Queue presentQueue = device.getQueue(presentQfIx, 0);

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
    vertexShaderFile.seekg(0);
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
    fragmentShaderFile.seekg(0);
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

    std::vector<vk::Viewport> viewports =
      { vk::Viewport(0, 0, swapchainExtent.width, swapchainExtent.height, 0, 1) };
    std::vector<vk::Rect2D> scissors = { vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent) };
    vk::PipelineViewportStateCreateInfo viewportInfo
      ({},
       viewports.size(), viewports.data(),
       scissors.size(), scissors.data()
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
           vk::BlendOp::eAdd
           )
      };
    std::array<float,4> blendConstants = {{ 0, 0, 0, 0 }};
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo
      ({},
       false,
       vk::LogicOp::eNoOp,
       colorBlendAttachments.size(),
       colorBlendAttachments.data(),
       blendConstants);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo ({}, {}, {});
    vk::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutInfo);

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
       pipelineLayout,
       renderpass,
       0,
       nullptr,
       -1);
    vk::Pipeline graphicsPipeline =
      device.createGraphicsPipeline(nullptr, graphicsPipelineInfo);

    device.destroyShaderModule(vertexShaderModule);
    device.destroyShaderModule(fragmentShaderModule);

    for (size_t i = 0; i < commandBuffers.size(); ++i) {
      auto &c = commandBuffers[i];
      vk::CommandBufferBeginInfo beginInfo({}, nullptr);
      c.begin(beginInfo);

      std::vector<vk::ClearValue> clearValues =
        { vk::ClearValue().setColor(vk::ClearColorValue().setFloat32({{ 0, 0, 0, 1 }}))
        };
      vk::RenderPassBeginInfo renderpassBeginInfo
        (renderpass,
         framebuffers[i],
         vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent),
         clearValues.size(),
         clearValues.data()
         );
      c.beginRenderPass(renderpassBeginInfo, vk::SubpassContents::eInline);

      c.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
      c.draw(3, 1, 0, 0);

      c.endRenderPass();

      c.end();
    }

    vk::Semaphore imageAvailableSem = device.createSemaphore(vk::SemaphoreCreateInfo());
    vk::Semaphore renderFinishedSem = device.createSemaphore(vk::SemaphoreCreateInfo());

    Context context =
      Context
        (window,
         instance,
         loader,
         messenger,
         surface,
         device,
         graphicsQueue,
         presentQueue,
         swapchain,
         renderpass,
         imageViews,
         framebuffers,
         commandPool,
         commandBuffers,
         pipelineLayout,
         graphicsPipeline,
         imageAvailableSem,
         renderFinishedSem);
    return context;
  }
};

int main() {
  vk::Optional<Context> context = Context::create(1280, 960, "triangle");

  while (!context->shouldClose()) {
    glfwPollEvents();

    context->drawFrame();
  }

  return 0;
}
