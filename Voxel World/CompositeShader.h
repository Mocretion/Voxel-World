#pragma once

#include "Shader.h"

/**
 * Shader responsible for final composition and post-processing.
 * Can be used for tone mapping, gamma correction, or simple copy operations.
 */
class CompositeShader : public Shader {
public:
    // Shader interface implementation
    void initialize(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) override;
    void cleanup(VkDevice device) override;
    void recreate(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) override;

    void recordCommands(VkCommandBuffer commandBuffer, uint32_t frameIndex) override;

    // Query methods
    bool needsVertexInput() const override { return false; } // Fullscreen triangle
    bool hasUniformData() const override { return false; }

    // Composite specific methods
    void setInputTextures(VulkanRenderer* renderer, VkImageView positionView, VkImageView normalView, VkImageView albedoView, VkSampler sampler);

protected:
    void createDescriptorSetLayout(VkDevice device) override;
    void createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent) override;
    void createDescriptorSets(VulkanRenderer* renderer) override;

private:
    // Input texture references (not owned by this shader)
    VkImageView inputPositionView = VK_NULL_HANDLE;
    VkImageView inputNormalView = VK_NULL_HANDLE;
    VkImageView inputAlbedoView = VK_NULL_HANDLE;
    VkSampler inputSampler = VK_NULL_HANDLE;
};