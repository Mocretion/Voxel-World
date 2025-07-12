#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>

// Forward declarations
class VulkanRenderer;

/**
 * Base class for all shader implementations in the rendering pipeline.
 * Each shader handles its own pipeline creation, uniform management, and command recording.
 */
class Shader {
public:
    virtual ~Shader() = default;

    // Core lifecycle methods
    virtual void initialize(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) = 0;
    virtual void cleanup(VkDevice device) = 0;
    virtual void recreate(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) = 0;

    // Per-frame operations
    virtual void updateUniforms(uint32_t frameIndex, const void* data) {}
    virtual void recordCommands(VkCommandBuffer commandBuffer, uint32_t frameIndex) = 0;

    // Getters
    VkPipeline getPipeline() const { return pipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

    // Query methods
    virtual bool needsVertexInput() const { return true; }
    virtual bool hasUniformData() const { return false; }
    virtual size_t getUniformDataSize() const { return 0; }

protected:
    // Core Vulkan objects managed by this shader
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> descriptorSets;

    // Uniform buffer management (if needed)
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    // Helper methods for common operations
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    void createUniformBuffers(VulkanRenderer* renderer, size_t bufferSize, size_t frameCount);
    void cleanupUniformBuffers(VkDevice device);

    // Virtual methods for shader-specific pipeline creation
    virtual void createDescriptorSetLayout(VkDevice device) = 0;
    virtual void createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent) = 0;
    virtual void createDescriptorSets(VulkanRenderer* renderer) = 0;

    // Utility for loading shader files
    std::vector<char> loadShaderFile(const std::string& filename);
};