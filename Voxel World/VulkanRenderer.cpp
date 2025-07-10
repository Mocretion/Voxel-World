#include "VulkanRenderer.h"
#include "ResourceManager.h"

#include "GBufferShader.h"
#include "SSAOShader.h"
#include "CompositeShader.h"
#include "SSAOBlurShader.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <set>

VulkanRenderer::VulkanRenderer()
{
    // All Vulkan handles are now initialized in the header file
    // Constructor - main initialization happens in initialize()
}

VulkanRenderer::~VulkanRenderer()
{
    cleanup();
}

void VulkanRenderer::initialize(SDL_Window* window, uint32_t windowWidth, uint32_t windowHeight)
{
    this->window = window;
    this->windowWidth = windowWidth;
    this->windowHeight = windowHeight;

    // Core Vulkan setup
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    // Swap chain and basic resources
    createSwapChain();
    createImageViews();

    // Render passes
    createRenderPasses();

    // Command and memory resources
    createCommandPool();
    createDepthResources();
    createGBufferResources();
    createSSAOResources();
    createGBufferSampler();

    // Framebuffers
    createFramebuffers();
    createGBufferFramebuffers();
    createSSAOFramebuffers();

    // Descriptor pool (shared by all shaders)
    createDescriptorPool();

    // Initialize shader pipeline
    initializeShaders();

    // Commands and synchronization
    createCommandBuffers();
    createSyncObjects();

    std::cout << "Vulkan renderer initialized successfully with SSAO blur pipeline!" << std::endl;
}

void VulkanRenderer::initializeShaders()
{
    std::cout << "Starting shader initialization..." << std::endl;

    try
    {
        // Create shader instances
        std::cout << "Creating GBufferShader..." << std::endl;
        gBufferShader = std::make_unique<GBufferShader>();

        std::cout << "Creating SSAOShader..." << std::endl;
        ssaoShader = std::make_unique<SSAOShader>();

        std::cout << "Creating Horizontal Blur Shader..." << std::endl;
        horizontalBlurShader = std::make_unique<SSAOBlurShader>();

        std::cout << "Creating Vertical Blur Shader..." << std::endl;
        verticalBlurShader = std::make_unique<SSAOBlurShader>();

        std::cout << "Creating additional blur shaders..." << std::endl;
        horizontalBlurShader2 = std::make_unique<SSAOBlurShader>();
        verticalBlurShader2 = std::make_unique<SSAOBlurShader>();

        std::cout << "Creating CompositeShader..." << std::endl;
        compositeShader = std::make_unique<CompositeShader>();

        // Initialize each shader
        std::cout << "Initializing GBufferShader..." << std::endl;
        gBufferShader->initialize(this, gBufferRenderPass, swapChainExtent);

        std::cout << "Initializing SSAOShader..." << std::endl;
        ssaoShader->initialize(this, ssaoRenderPass, swapChainExtent);

        std::cout << "Initializing Horizontal Blur Shader..." << std::endl;
        horizontalBlurShader->initialize(this, blurRenderPass, swapChainExtent);

        std::cout << "Initializing Vertical Blur Shader..." << std::endl;
        verticalBlurShader->initialize(this, blurRenderPass, swapChainExtent);

        std::cout << "Initializing additional blur shaders..." << std::endl;
        horizontalBlurShader2->initialize(this, blurRenderPass, swapChainExtent);
        verticalBlurShader2->initialize(this, blurRenderPass, swapChainExtent);

        std::cout << "Initializing CompositeShader..." << std::endl;
        compositeShader->initialize(this, renderPass, swapChainExtent);

        // Configure SSAO shader with G-buffer textures
        std::cout << "Configuring SSAO shader..." << std::endl;
        ssaoShader->setGBufferTextures(
            this,
            gBuffer.positionImageView,
            gBuffer.normalImageView,
            gBuffer.albedoImageView,
            gBufferSampler
        );

        // Configure horizontal blur shader
        std::cout << "Configuring horizontal blur shader..." << std::endl;
        horizontalBlurShader->setBlurDirection(glm::vec2(1.0f, 0.0f));  // Horizontal
        horizontalBlurShader->setBlurRadius(2.5f);
        horizontalBlurShader->setDepthThreshold(0.08f);
        horizontalBlurShader->setInputTextures(
            this,
            ssaoBuffers.rawSSAOImageView,     // Input: raw SSAO
            gBuffer.positionImageView,        // Depth for edge detection
            gBufferSampler
        );

        // Configure vertical blur shader
        std::cout << "Configuring vertical blur shader..." << std::endl;
        verticalBlurShader->setBlurDirection(glm::vec2(0.0f, 1.0f));   // Vertical
        verticalBlurShader->setBlurRadius(2.5f);
        verticalBlurShader->setDepthThreshold(0.08f);
        verticalBlurShader->setInputTextures(
            this,
            ssaoBuffers.blurImage1View,       // Input: horizontal blur result
            gBuffer.positionImageView,        // Depth for edge detection
            gBufferSampler
        );

        // Configure the additional blur shaders (after first cycle completes)
        horizontalBlurShader2->setBlurDirection(glm::vec2(1.0f, 0.0f));  // Horizontal
        horizontalBlurShader2->setBlurRadius(4.0f);
        horizontalBlurShader2->setDepthThreshold(0.05f);
        horizontalBlurShader2->setInputTextures(
            this,
            ssaoBuffers.blurImage2View,        // Input: first cycle result
            gBuffer.positionImageView,         // Depth for edge detection
            gBufferSampler
        );

        verticalBlurShader2->setBlurDirection(glm::vec2(0.0f, 1.0f));   // Vertical
        verticalBlurShader2->setBlurRadius(4.0f);
        verticalBlurShader2->setDepthThreshold(0.05f);
        verticalBlurShader2->setInputTextures(
            this,
            ssaoBuffers.blurImage1View,        // Input: second horizontal result
            gBuffer.positionImageView,         // Depth for edge detection
            gBufferSampler
        );

        // Configure composite shader with G-buffer and final blurred SSAO
        std::cout << "Configuring composite shader..." << std::endl;
        compositeShader->setInputTextures(
            this,
            gBuffer.positionImageView,
            gBuffer.normalImageView,
            gBuffer.albedoImageView,
            ssaoBuffers.blurImage2View,       // Final vertical blur result
            gBufferSampler
        );

        std::cout << "SSAO blur shader pipeline initialized successfully!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Shader initialization failed: " << e.what() << std::endl;
        throw;
    }
}

bool VulkanRenderer::isWindowMinimized() const
{
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    return width == 0 || height == 0;
}

bool VulkanRenderer::beginFrame()
{
    // Wait for previous frame to finish
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // Acquire next image from swap chain
    VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
        framebufferResized = false;
        recreateSwapChain();
        return false; // Skip this frame
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Check if a previous frame is using this image
    if (imagesInFlight[currentImageIndex] != VK_NULL_HANDLE)
    {
        vkWaitForFences(device, 1, &imagesInFlight[currentImageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight[currentImageIndex] = inFlightFences[currentFrame];

    // Reset fence for this frame
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // Update buffers if mesh data changed
    if (meshDataDirty)
    {
        createVertexBuffer();
        createIndexBuffer();
        meshDataDirty = false;
    }

    // Record command buffer
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], currentImageIndex);

    return true;
}

void VulkanRenderer::endFrame()
{
    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present the rendered image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentImageIndex;  // Use the image index from beginFrame()

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
        framebufferResized = false;
        std::cout << "[DEBUG] Calling recreateSwapChain from endFrame() - present result was out of date" << std::endl;
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::updateMeshData(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
    currentVertices = vertices;
    currentIndices = indices;
    meshDataDirty = true;
}

void VulkanRenderer::updateUniforms(const UniformBufferObject& ubo)
{
    if (gBufferShader)
    {
        gBufferShader->updateUniforms(currentFrame, &ubo);
    }
}

void VulkanRenderer::updateSSAOUniforms(const Camera& camera, float aspectRatio)
{
    static int updateCount = 0;
    if (updateCount++ % 60 == 0)
    {  // Print every 60 frames
        std::cout << "VulkanRenderer: Updating SSAO uniforms, aspect ratio: " << aspectRatio << std::endl;
    }

    if (ssaoShader)
    {
        ssaoShader->updateSSAOParams(camera, aspectRatio, swapChainExtent);
    }
}

void VulkanRenderer::handleResize(uint32_t newWidth, uint32_t newHeight)
{
    std::cout << "[DEBUG] VulkanRenderer::handleResize called with " << newWidth << "x" << newHeight << std::endl;

    // Update the stored window dimensions
    windowWidth = newWidth;
    windowHeight = newHeight;
    framebufferResized = true;

    std::cout << "[DEBUG] Window dimensions updated and framebufferResized flag set" << std::endl;
}

void VulkanRenderer::toggleWireframe()
{
    if (gBufferShader)
    {
        bool currentMode = gBufferShader->getWireframeMode();
        gBufferShader->setWireframeMode(!currentMode);

        // Recreate the G-buffer shader pipeline with new wireframe setting
        gBufferShader->recreate(this, gBufferRenderPass, swapChainExtent);

        std::cout << "Wireframe mode " << (currentMode ? "disabled" : "enabled") << std::endl;
    }
}

void VulkanRenderer::cleanup()
{
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);

        // Cleanup all shaders
        if (gBufferShader)
        {
            gBufferShader->cleanup(device);
            gBufferShader.reset();
        }
        if (ssaoShader)
        {
            ssaoShader->cleanup(device);
            ssaoShader.reset();
        }
        if (horizontalBlurShader)
        {
            horizontalBlurShader->cleanup(device);
            horizontalBlurShader.reset();
        }
        if (verticalBlurShader)
        {
            verticalBlurShader->cleanup(device);
            verticalBlurShader.reset();
        }
        if (horizontalBlurShader2)
        {
            horizontalBlurShader2->cleanup(device);
            horizontalBlurShader2.reset();
        }
        if (verticalBlurShader2)
        {
            verticalBlurShader2->cleanup(device);
            verticalBlurShader2.reset();
        }
        if (compositeShader)
        {
            compositeShader->cleanup(device);
            compositeShader.reset();
        }

        cleanupSwapChain();

        if (vertexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, vertexBuffer, nullptr);
            vkFreeMemory(device, vertexBufferMemory, nullptr);
        }

        if (indexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, indexBuffer, nullptr);
            vkFreeMemory(device, indexBufferMemory, nullptr);
        }

        if (descriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            }
            if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            }
            if (inFlightFences[i] != VK_NULL_HANDLE)
            {
                vkDestroyFence(device, inFlightFences[i], nullptr);
            }
        }

        if (commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }

        if (device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(device, nullptr);
        }

        if (surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }

        if (instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(instance, nullptr);
        }
    }
}

void VulkanRenderer::createInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Voxel World";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Get required extensions from SDL
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}

void VulkanRenderer::createSurface()
{
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface))
    {
        throw std::runtime_error("Failed to create Vulkan surface!");
    }
}

void VulkanRenderer::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

void VulkanRenderer::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.fillModeNonSolid = VK_TRUE; // Enable wireframe rendering

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

// [Continuing with the rest of the implementation...]

VulkanRenderer::QueueFamilyIndices VulkanRenderer::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

bool VulkanRenderer::isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = true; // Simplified - should check for swapchain extension

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.fillModeNonSolid;
}

VulkanRenderer::SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void VulkanRenderer::createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    // First, try to find standard RGBA format without sRGB conversion
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

    // If that's not available, try the original sRGB format
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    // Fallback to first available format
    return availableFormats[0];
}

VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    std::cout << "[DEBUG] chooseSwapExtent called" << std::endl;
    std::cout << "[DEBUG] Current extent from capabilities: " << capabilities.currentExtent.width << "x" << capabilities.currentExtent.height << std::endl;
    std::cout << "[DEBUG] Stored window dimensions: " << windowWidth << "x" << windowHeight << std::endl;

    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        std::cout << "[DEBUG] Using capabilities.currentExtent" << std::endl;
        return capabilities.currentExtent;
    }
    else
    {
        VkExtent2D actualExtent = { windowWidth, windowHeight };
        std::cout << "[DEBUG] Using stored window dimensions: " << actualExtent.width << "x" << actualExtent.height << std::endl;

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        std::cout << "[DEBUG] Final extent after clamping: " << actualExtent.width << "x" << actualExtent.height << std::endl;
        return actualExtent;
    }
}

void VulkanRenderer::createImageViews()
{
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

void VulkanRenderer::createRenderPasses()
{
    createScreenRenderPass();
    createGBufferRenderPass();
    createSSAORenderPass();
    createBlurRenderPass();
}

void VulkanRenderer::createFramebuffers()
{
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

void VulkanRenderer::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool!");
    }
}

void VulkanRenderer::createVertexBuffer()
{
    if (currentVertices.empty())
    {
        return; // No vertices to upload
    }

    VkDeviceSize bufferSize = sizeof(currentVertices[0]) * currentVertices.size();

    // Clean up old buffer if it exists
    if (vertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, currentVertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::createIndexBuffer()
{
    if (currentIndices.empty())
    {
        return; // No indices to upload
    }

    VkDeviceSize bufferSize = sizeof(currentIndices[0]) * currentIndices.size();

    // Clean up old buffer if it exists
    if (indexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, currentIndices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void VulkanRenderer::createDescriptorPool()
{
    // Calculate descriptor requirements for all 5 shaders:
    // 1. GBufferShader - 1 uniform buffer
    // 2. SSAOShader - 1 uniform buffer + 4 image samplers (G-buffer + noise)
    // 3. HorizontalBlurShader - 1 uniform buffer + 2 image samplers (SSAO + depth)
    // 4. VerticalBlurShader - 1 uniform buffer + 2 image samplers (blur1 + depth)  
    // 5. CompositeShader - 4 image samplers (G-buffer + final SSAO)

    std::array<VkDescriptorPoolSize, 2> poolSizes{};

    // Uniform buffers: GBuffer(1) + SSAO(1) + HorizontalBlur(1) + VerticalBlur(1) = 4 per frame
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 6);

    // Image samplers: SSAO(4) + HorizontalBlur(2) + VerticalBlur(2) + Composite(4) = 12 per frame
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 16);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 7); // 5 shaders

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    std::cout << "Descriptor pool created for 5-shader SSAO blur pipeline!" << std::endl;
}

void VulkanRenderer::createCommandBuffers()
{
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void VulkanRenderer::createSyncObjects()
{
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create synchronization objects for a frame!");
        }
    }
}

void VulkanRenderer::createDepthResources()
{
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    // PASS 1: Render to G-buffer
    VkRenderPassBeginInfo gBufferRenderPassInfo{};
    gBufferRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    gBufferRenderPassInfo.renderPass = gBufferRenderPass;
    gBufferRenderPassInfo.framebuffer = gBufferFramebuffers[imageIndex];
    gBufferRenderPassInfo.renderArea.offset = { 0, 0 };
    gBufferRenderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 4> gBufferClearValues{};
    gBufferClearValues[0].color = { {0.0f, 0.0f, 0.0f, 0.0f} };  // Position
    gBufferClearValues[1].color = { {0.0f, 0.0f, 0.0f, 0.0f} };  // Normal  
    gBufferClearValues[2].color = { {0.0f, 0.0f, 0.0f, 0.0f} };  // Albedo
    gBufferClearValues[3].depthStencil = { 1.0f, 0 };            // Depth

    gBufferRenderPassInfo.clearValueCount = static_cast<uint32_t>(gBufferClearValues.size());
    gBufferRenderPassInfo.pClearValues = gBufferClearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &gBufferRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    gBufferShader->recordCommands(commandBuffer, currentFrame);
    if (vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE && !currentIndices.empty())
    {
        gBufferShader->bindVertexData(commandBuffer, vertexBuffer, indexBuffer);
        gBufferShader->drawIndexed(commandBuffer, static_cast<uint32_t>(currentIndices.size()));
    }

    vkCmdEndRenderPass(commandBuffer);

    // PASS 2: Generate raw SSAO
    VkRenderPassBeginInfo ssaoRenderPassInfo{};
    ssaoRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    ssaoRenderPassInfo.renderPass = ssaoRenderPass;
    ssaoRenderPassInfo.framebuffer = ssaoFramebuffers[imageIndex];
    ssaoRenderPassInfo.renderArea.offset = { 0, 0 };
    ssaoRenderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue ssaoClearValue{};
    ssaoClearValue.color = { {1.0f, 0.0f, 0.0f, 0.0f} };  // Clear to white (no occlusion)
    ssaoRenderPassInfo.clearValueCount = 1;
    ssaoRenderPassInfo.pClearValues = &ssaoClearValue;

    vkCmdBeginRenderPass(commandBuffer, &ssaoRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    ssaoShader->recordCommands(commandBuffer, currentFrame);
    vkCmdEndRenderPass(commandBuffer);

    // PASS 3: Horizontal blur
    VkRenderPassBeginInfo blurRenderPassInfo1{};
    blurRenderPassInfo1.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    blurRenderPassInfo1.renderPass = blurRenderPass;
    blurRenderPassInfo1.framebuffer = blurFramebuffers1[imageIndex];
    blurRenderPassInfo1.renderArea.offset = { 0, 0 };
    blurRenderPassInfo1.renderArea.extent = swapChainExtent;

    VkClearValue blurClearValue{};
    blurClearValue.color = { {1.0f, 0.0f, 0.0f, 0.0f} };
    blurRenderPassInfo1.clearValueCount = 1;
    blurRenderPassInfo1.pClearValues = &blurClearValue;

    vkCmdBeginRenderPass(commandBuffer, &blurRenderPassInfo1, VK_SUBPASS_CONTENTS_INLINE);
    horizontalBlurShader->recordCommands(commandBuffer, currentFrame);
    vkCmdEndRenderPass(commandBuffer);

    // PASS 4: Vertical blur
    VkRenderPassBeginInfo blurRenderPassInfo2{};
    blurRenderPassInfo2.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    blurRenderPassInfo2.renderPass = blurRenderPass;
    blurRenderPassInfo2.framebuffer = blurFramebuffers2[imageIndex];
    blurRenderPassInfo2.renderArea.offset = { 0, 0 };
    blurRenderPassInfo2.renderArea.extent = swapChainExtent;

    blurRenderPassInfo2.clearValueCount = 1;
    blurRenderPassInfo2.pClearValues = &blurClearValue;

    vkCmdBeginRenderPass(commandBuffer, &blurRenderPassInfo2, VK_SUBPASS_CONTENTS_INLINE);
    verticalBlurShader->recordCommands(commandBuffer, currentFrame);
    vkCmdEndRenderPass(commandBuffer);

    // PASS 5: Second Horizontal Pass (blur2 → blur1)
    VkRenderPassBeginInfo blurRenderPassInfo3{};
    blurRenderPassInfo3.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    blurRenderPassInfo3.renderPass = blurRenderPass;
    blurRenderPassInfo3.framebuffer = blurFramebuffers1[imageIndex];
    blurRenderPassInfo3.renderArea.offset = { 0, 0 };
    blurRenderPassInfo3.renderArea.extent = swapChainExtent;
    blurRenderPassInfo3.clearValueCount = 1;
    blurRenderPassInfo3.pClearValues = &blurClearValue;  // Reuse existing blurClearValue

    vkCmdBeginRenderPass(commandBuffer, &blurRenderPassInfo3, VK_SUBPASS_CONTENTS_INLINE);
    horizontalBlurShader2->recordCommands(commandBuffer, currentFrame);
    vkCmdEndRenderPass(commandBuffer);

    // PASS 6: Second Vertical Pass (blur1 → blur2)
    VkRenderPassBeginInfo blurRenderPassInfo4{};
    blurRenderPassInfo4.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    blurRenderPassInfo4.renderPass = blurRenderPass;
    blurRenderPassInfo4.framebuffer = blurFramebuffers2[imageIndex];
    blurRenderPassInfo4.renderArea.offset = { 0, 0 };
    blurRenderPassInfo4.renderArea.extent = swapChainExtent;
    blurRenderPassInfo4.clearValueCount = 1;
    blurRenderPassInfo4.pClearValues = &blurClearValue;  // Reuse existing blurClearValue

    vkCmdBeginRenderPass(commandBuffer, &blurRenderPassInfo4, VK_SUBPASS_CONTENTS_INLINE);
    verticalBlurShader2->recordCommands(commandBuffer, currentFrame);
    vkCmdEndRenderPass(commandBuffer);

    // PASS 7: Final composite to screen
    VkRenderPassBeginInfo screenRenderPassInfo{};
    screenRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    screenRenderPassInfo.renderPass = renderPass;
    screenRenderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    screenRenderPassInfo.renderArea.offset = { 0, 0 };
    screenRenderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue screenClearValue{};
    screenClearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    screenRenderPassInfo.clearValueCount = 1;
    screenRenderPassInfo.pClearValues = &screenClearValue;

    vkCmdBeginRenderPass(commandBuffer, &screenRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    compositeShader->recordCommands(commandBuffer, currentFrame);
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer!");
    }
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void VulkanRenderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

VkFormat VulkanRenderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

VkFormat VulkanRenderer::findDepthFormat()
{
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool VulkanRenderer::hasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create texture image view!");
    }

    return imageView;
}

void VulkanRenderer::recreateSwapChain()
{
    if (isWindowMinimized())
    {
        return;
    }

    int width = 0, height = 0;
    SDL_GetWindowSize(window, &width, &height);
    windowWidth = static_cast<uint32_t>(width);
    windowHeight = static_cast<uint32_t>(height);

    while (width == 0 || height == 0)
    {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createGBufferResources();
    createSSAOResources();
    createFramebuffers();
    createGBufferFramebuffers();
    createSSAOFramebuffers();

    // Recreate all shaders with new extent
    if (gBufferShader)
    {
        gBufferShader->recreate(this, gBufferRenderPass, swapChainExtent);
    }
    if (ssaoShader)
    {
        ssaoShader->recreate(this, ssaoRenderPass, swapChainExtent);
        ssaoShader->setGBufferTextures(this, gBuffer.positionImageView, gBuffer.normalImageView, gBuffer.albedoImageView, gBufferSampler);
    }
    if (horizontalBlurShader)
    {
        horizontalBlurShader->recreate(this, blurRenderPass, swapChainExtent);
        horizontalBlurShader->setBlurDirection(glm::vec2(1.0f, 0.0f));
        horizontalBlurShader->setBlurRadius(4.0f);
        horizontalBlurShader->setDepthThreshold(0.1f);
        horizontalBlurShader->setInputTextures(this, ssaoBuffers.rawSSAOImageView, gBuffer.positionImageView, gBufferSampler);
    }
    if (verticalBlurShader)
    {
        verticalBlurShader->recreate(this, blurRenderPass, swapChainExtent);
        verticalBlurShader->setBlurDirection(glm::vec2(0.0f, 1.0f));
        verticalBlurShader->setBlurRadius(4.0f);
        verticalBlurShader->setDepthThreshold(0.1f);
        verticalBlurShader->setInputTextures(this, ssaoBuffers.blurImage1View, gBuffer.positionImageView, gBufferSampler);
    }
    if (compositeShader)
    {
        compositeShader->recreate(this, renderPass, swapChainExtent);
        compositeShader->setInputTextures(this, gBuffer.positionImageView, gBuffer.normalImageView,
            gBuffer.albedoImageView, ssaoBuffers.blurImage2View, gBufferSampler);
    }

    std::cout << "Swap chain and SSAO blur pipeline recreated successfully!" << std::endl;
}

void VulkanRenderer::cleanupSwapChain()
{
    cleanupGBuffer();
    cleanupSSAOBuffers();

    if (gBufferSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, gBufferSampler, nullptr);
        gBufferSampler = VK_NULL_HANDLE;
    }

    if (depthImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, depthImageView, nullptr);
    }
    if (depthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, depthImage, nullptr);
    }
    if (depthImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, depthImageMemory, nullptr);
    }

    for (auto framebuffer : swapChainFramebuffers)
    {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews)
    {
        vkDestroyImageView(device, imageView, nullptr);
    }

    if (swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
}

void VulkanRenderer::createGBufferRenderPass()
{
    std::array<VkAttachmentDescription, 4> attachments = {};

    // Position attachment (attachment 0)
    attachments[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Normal attachment (attachment 1)
    attachments[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Albedo attachment (attachment 2)
    attachments[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attachments[2].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment (attachment 3)
    attachments[3].format = findDepthFormat();
    attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Color attachment references
    std::array<VkAttachmentReference, 3> colorAttachmentRefs = {};
    colorAttachmentRefs[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // Position
    colorAttachmentRefs[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // Normal
    colorAttachmentRefs[2] = { 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // Albedo

    // Depth attachment reference
    VkAttachmentReference depthAttachmentRef = {};
    depthAttachmentRef.attachment = 3;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass description
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
    subpass.pColorAttachments = colorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependency
    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Create render pass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &gBufferRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create G-buffer render pass!");
    }
}

void VulkanRenderer::createGBufferResources()
{
    // Create position buffer
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        gBuffer.positionImage, gBuffer.positionImageMemory);

    gBuffer.positionImageView = createImageView(gBuffer.positionImage,
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create normal buffer
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        gBuffer.normalImage, gBuffer.normalImageMemory);

    gBuffer.normalImageView = createImageView(gBuffer.normalImage,
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

    // Create albedo buffer
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        gBuffer.albedoImage, gBuffer.albedoImageMemory);

    gBuffer.albedoImageView = createImageView(gBuffer.albedoImage,
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    std::cout << "G-buffer resources created successfully!" << std::endl;
}

void VulkanRenderer::createGBufferFramebuffers()
{
    gBufferFramebuffers.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        std::array<VkImageView, 4> attachments = {
            gBuffer.positionImageView,  // Position
            gBuffer.normalImageView,    // Normal
            gBuffer.albedoImageView,    // Albedo
            depthImageView              // Depth (shared with regular rendering)
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = gBufferRenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &gBufferFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create G-buffer framebuffer!");
        }
    }

    std::cout << "G-buffer framebuffers created successfully!" << std::endl;
}

void VulkanRenderer::cleanupGBuffer()
{
    // Destroy G-buffer framebuffers
    for (auto framebuffer : gBufferFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    gBufferFramebuffers.clear();

    // Destroy G-buffer image views
    if (gBuffer.positionImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, gBuffer.positionImageView, nullptr);
        gBuffer.positionImageView = VK_NULL_HANDLE;
    }
    if (gBuffer.normalImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, gBuffer.normalImageView, nullptr);
        gBuffer.normalImageView = VK_NULL_HANDLE;
    }
    if (gBuffer.albedoImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, gBuffer.albedoImageView, nullptr);
        gBuffer.albedoImageView = VK_NULL_HANDLE;
    }

    // Destroy G-buffer images and memory
    if (gBuffer.positionImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, gBuffer.positionImage, nullptr);
        gBuffer.positionImage = VK_NULL_HANDLE;
    }
    if (gBuffer.positionImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, gBuffer.positionImageMemory, nullptr);
        gBuffer.positionImageMemory = VK_NULL_HANDLE;
    }

    if (gBuffer.normalImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, gBuffer.normalImage, nullptr);
        gBuffer.normalImage = VK_NULL_HANDLE;
    }
    if (gBuffer.normalImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, gBuffer.normalImageMemory, nullptr);
        gBuffer.normalImageMemory = VK_NULL_HANDLE;
    }

    if (gBuffer.albedoImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, gBuffer.albedoImage, nullptr);
        gBuffer.albedoImage = VK_NULL_HANDLE;
    }
    if (gBuffer.albedoImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, gBuffer.albedoImageMemory, nullptr);
        gBuffer.albedoImageMemory = VK_NULL_HANDLE;
    }

    // Destroy G-buffer render pass
    if (gBufferRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, gBufferRenderPass, nullptr);
        gBufferRenderPass = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createGBufferSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST; // No filtering for G-buffer
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &gBufferSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create G-buffer sampler!");
    }
}

void VulkanRenderer::createScreenRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create screen render pass!");
    }
}

void VulkanRenderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

void VulkanRenderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer VulkanRenderer::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanRenderer::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanRenderer::createSSAOResources()
{
    std::cout << "Creating SSAO intermediate textures..." << std::endl;

    // Raw SSAO texture (R8_UNORM for single channel)
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        ssaoBuffers.rawSSAOImage, ssaoBuffers.rawSSAOImageMemory);

    ssaoBuffers.rawSSAOImageView = createImageView(ssaoBuffers.rawSSAOImage,
        VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    // Blur texture 1 (ping)
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        ssaoBuffers.blurImage1, ssaoBuffers.blurImage1Memory);

    ssaoBuffers.blurImage1View = createImageView(ssaoBuffers.blurImage1,
        VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    // Blur texture 2 (pong)
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_FORMAT_R8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        ssaoBuffers.blurImage2, ssaoBuffers.blurImage2Memory);

    ssaoBuffers.blurImage2View = createImageView(ssaoBuffers.blurImage2,
        VK_FORMAT_R8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    std::cout << "SSAO intermediate textures created successfully!" << std::endl;
}

void VulkanRenderer::createSSAOFramebuffers()
{
    std::cout << "Creating SSAO framebuffers..." << std::endl;

    // SSAO framebuffers (raw SSAO output)
    ssaoFramebuffers.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        VkImageView attachments[] = { ssaoBuffers.rawSSAOImageView };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = ssaoRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &ssaoFramebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create SSAO framebuffer!");
        }
    }

    // Blur framebuffers 1 (horizontal blur output)
    blurFramebuffers1.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        VkImageView attachments[] = { ssaoBuffers.blurImage1View };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = blurRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &blurFramebuffers1[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create blur framebuffer 1!");
        }
    }

    // Blur framebuffers 2 (vertical blur output)
    blurFramebuffers2.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        VkImageView attachments[] = { ssaoBuffers.blurImage2View };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = blurRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &blurFramebuffers2[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create blur framebuffer 2!");
        }
    }

    std::cout << "SSAO framebuffers created successfully!" << std::endl;
}

void VulkanRenderer::createSSAORenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &ssaoRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SSAO render pass!");
    }
}

void VulkanRenderer::createBlurRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8_UNORM;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &blurRenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create blur render pass!");
    }
}

void VulkanRenderer::cleanupSSAOBuffers()
{
    // Destroy SSAO framebuffers
    for (auto framebuffer : ssaoFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    ssaoFramebuffers.clear();

    for (auto framebuffer : blurFramebuffers1)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    blurFramebuffers1.clear();

    for (auto framebuffer : blurFramebuffers2)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    blurFramebuffers2.clear();

    // Destroy SSAO image views
    if (ssaoBuffers.rawSSAOImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, ssaoBuffers.rawSSAOImageView, nullptr);
        ssaoBuffers.rawSSAOImageView = VK_NULL_HANDLE;
    }
    if (ssaoBuffers.blurImage1View != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, ssaoBuffers.blurImage1View, nullptr);
        ssaoBuffers.blurImage1View = VK_NULL_HANDLE;
    }
    if (ssaoBuffers.blurImage2View != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, ssaoBuffers.blurImage2View, nullptr);
        ssaoBuffers.blurImage2View = VK_NULL_HANDLE;
    }

    // Destroy SSAO images and memory
    if (ssaoBuffers.rawSSAOImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, ssaoBuffers.rawSSAOImage, nullptr);
        ssaoBuffers.rawSSAOImage = VK_NULL_HANDLE;
    }
    if (ssaoBuffers.rawSSAOImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, ssaoBuffers.rawSSAOImageMemory, nullptr);
        ssaoBuffers.rawSSAOImageMemory = VK_NULL_HANDLE;
    }

    if (ssaoBuffers.blurImage1 != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, ssaoBuffers.blurImage1, nullptr);
        ssaoBuffers.blurImage1 = VK_NULL_HANDLE;
    }
    if (ssaoBuffers.blurImage1Memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, ssaoBuffers.blurImage1Memory, nullptr);
        ssaoBuffers.blurImage1Memory = VK_NULL_HANDLE;
    }

    if (ssaoBuffers.blurImage2 != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, ssaoBuffers.blurImage2, nullptr);
        ssaoBuffers.blurImage2 = VK_NULL_HANDLE;
    }
    if (ssaoBuffers.blurImage2Memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, ssaoBuffers.blurImage2Memory, nullptr);
        ssaoBuffers.blurImage2Memory = VK_NULL_HANDLE;
    }

    // Destroy SSAO render passes
    if (ssaoRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, ssaoRenderPass, nullptr);
        ssaoRenderPass = VK_NULL_HANDLE;
    }
    if (blurRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device, blurRenderPass, nullptr);
        blurRenderPass = VK_NULL_HANDLE;
    }
}