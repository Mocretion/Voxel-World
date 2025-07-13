#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <optional>
#include <memory>

#include "Vertex.h"
#include "Camera.h"

// Forward declarations for shader classes
class GBufferShader;
class SSAOShader;
class SSAOBlurShader;
class CompositeShader;
class Chunk;

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    // Initialize Vulkan with the given window
    void initialize(SDL_Window* window, uint32_t windowWidth, uint32_t windowHeight);
    void cleanup();

    // Frame rendering
    bool beginFrame();
    void endFrame();

    // NEW: Per-chunk rendering system (replaces updateMeshData)
    void renderChunks(const std::vector<Chunk*>& chunks);

    // Update uniform buffer (for camera matrices)
    void updateUniforms(const UniformBufferObject& ubo);
    void updateSSAOUniforms(const Camera& camera, float aspectRatio);

    // Handle window resize
    void handleResize(uint32_t newWidth, uint32_t newHeight);

    // Toggle wireframe mode
    void toggleWireframe();

    // Get current frame dimensions
    uint32_t getFrameWidth() const { return swapChainExtent.width; }
    uint32_t getFrameHeight() const { return swapChainExtent.height; }

    // Public interface for shaders and chunks
    VkDevice getDevice() const { return device; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDescriptorPool getDescriptorPool() const { return descriptorPool; }
    size_t getMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }

    // Helper methods for shaders and chunks
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
        VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
        VkImage& image, VkDeviceMemory& imageMemory);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    // Command buffer utilities
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

private:
    // Window settings
    SDL_Window* window = nullptr;
    uint32_t windowWidth;
    uint32_t windowHeight;
    const int MAX_FRAMES_IN_FLIGHT = 2;
    bool framebufferResized = false;

    // Vulkan core objects
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    // Depth buffer
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // G-buffer (Geometry Buffer) for deferred rendering
    struct GBuffer {
        VkImage positionImage = VK_NULL_HANDLE;
        VkDeviceMemory positionImageMemory = VK_NULL_HANDLE;
        VkImageView positionImageView = VK_NULL_HANDLE;

        VkImage normalImage = VK_NULL_HANDLE;
        VkDeviceMemory normalImageMemory = VK_NULL_HANDLE;
        VkImageView normalImageView = VK_NULL_HANDLE;

        VkImage albedoImage = VK_NULL_HANDLE;
        VkDeviceMemory albedoImageMemory = VK_NULL_HANDLE;
        VkImageView albedoImageView = VK_NULL_HANDLE;
    } gBuffer;

    // SSAO intermediate textures
    struct SSAOBuffers {
        // Raw SSAO output
        VkImage rawSSAOImage = VK_NULL_HANDLE;
        VkDeviceMemory rawSSAOImageMemory = VK_NULL_HANDLE;
        VkImageView rawSSAOImageView = VK_NULL_HANDLE;

        // Blur ping-pong textures
        VkImage blurImage1 = VK_NULL_HANDLE;
        VkDeviceMemory blurImage1Memory = VK_NULL_HANDLE;
        VkImageView blurImage1View = VK_NULL_HANDLE;

        VkImage blurImage2 = VK_NULL_HANDLE;
        VkDeviceMemory blurImage2Memory = VK_NULL_HANDLE;
        VkImageView blurImage2View = VK_NULL_HANDLE;
    } ssaoBuffers;

    // Swap chain
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    // G-buffer framebuffers
    std::vector<VkFramebuffer> gBufferFramebuffers;

    // SSAO framebuffers
    std::vector<VkFramebuffer> ssaoFramebuffers;
    std::vector<VkFramebuffer> blurFramebuffers1;
    std::vector<VkFramebuffer> blurFramebuffers2;

    // Render passes
    VkRenderPass renderPass = VK_NULL_HANDLE;          // Final screen render pass
    VkRenderPass gBufferRenderPass = VK_NULL_HANDLE;   // G-buffer render pass
    VkRenderPass ssaoRenderPass = VK_NULL_HANDLE;      // SSAO render pass
    VkRenderPass blurRenderPass = VK_NULL_HANDLE;      // Blur render pass

    // Commands and synchronization
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    uint32_t currentFrame = 0;

    // Descriptor pool (shared by all shaders)
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    // G-buffer sampler
    VkSampler gBufferSampler = VK_NULL_HANDLE;

    // REMOVED: Combined vertex/index buffers (no longer needed)
    // VkBuffer vertexBuffer = VK_NULL_HANDLE;
    // VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    // VkBuffer indexBuffer = VK_NULL_HANDLE;
    // VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    // NEW: Store chunks for rendering
    std::vector<Chunk*> chunksToRender;
    bool chunkDataDirty = false;

    uint32_t currentImageIndex = 0;

    // Shader pipeline instances
    std::unique_ptr<GBufferShader> gBufferShader;
    std::unique_ptr<SSAOShader> ssaoShader;
    std::unique_ptr<SSAOBlurShader> horizontalBlurShader;
    std::unique_ptr<SSAOBlurShader> verticalBlurShader;
    std::unique_ptr<SSAOBlurShader> horizontalBlurShader2;
    std::unique_ptr<SSAOBlurShader> verticalBlurShader2;
    std::unique_ptr<CompositeShader> compositeShader;

    // Helper structs
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // Initialization methods
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPasses();
    void createGBufferResources();
    void createSSAOResources();
    void createGBufferFramebuffers();
    void createSSAOFramebuffers();
    void createCommandPool();
    void createDescriptorPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createDepthResources();
    void createGBufferSampler();
    void initializeShaders();

    // Helper methods
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);

    void recreateSwapChain();
    void cleanupSwapChain();
    void cleanupGBuffer();
    void cleanupSSAOBuffers();
    bool isWindowMinimized() const;

    // Render pass creation
    void createScreenRenderPass();
    void createGBufferRenderPass();
    void createSSAORenderPass();
    void createBlurRenderPass();
    void createFramebuffers();

    // NEW: Per-chunk rendering helpers
    void renderGBufferPass(VkCommandBuffer commandBuffer);
    void uploadChunksToGPU();
};