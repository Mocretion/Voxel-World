#pragma once

#include "Shader.h"

/**
 * Shader responsible for bilateral blurring of SSAO to reduce noise while preserving edges.
 * Performs two-pass separable blur (horizontal + vertical) with depth-based edge detection.
 */
class SSAOBlurShader : public Shader {
public:
    struct BlurParams {
        glm::vec2 blurDirection;    // (1,0) for horizontal, (0,1) for vertical
        float blurRadius;           // Blur kernel radius in pixels
        float depthThreshold;       // Depth difference threshold for edge detection
        float padding[1];           // Align to 16 bytes
    };

    // Shader interface implementation
    void initialize(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) override;
    void cleanup(VkDevice device) override;
    void recreate(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) override;

    void updateUniforms(uint32_t frameIndex, const void* data) override;
    void recordCommands(VkCommandBuffer commandBuffer, uint32_t frameIndex) override;

    // Query methods
    bool needsVertexInput() const override { return false; } // Fullscreen triangle
    bool hasUniformData() const override { return true; }
    size_t getUniformDataSize() const override { return sizeof(BlurParams); }

    // Blur specific methods
    void setBlurDirection(const glm::vec2& direction);
    void setBlurRadius(float radius);
    void setDepthThreshold(float threshold);
    void setInputTextures(VulkanRenderer* renderer, VkImageView ssaoView, VkImageView depthView, VkSampler sampler);

protected:
    void createDescriptorSetLayout(VkDevice device) override;
    void createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent) override;
    void createDescriptorSets(VulkanRenderer* renderer) override;

private:
    // Input texture references (not owned by this shader)
    VkImageView inputSSAOView = VK_NULL_HANDLE;
    VkImageView inputDepthView = VK_NULL_HANDLE;
    VkSampler inputSampler = VK_NULL_HANDLE;

    // Current blur parameters
    BlurParams currentParams = {};
};