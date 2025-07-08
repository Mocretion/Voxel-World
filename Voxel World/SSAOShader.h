#pragma once

#include "Shader.h"
#include "Camera.h"

/**
 * Shader responsible for computing Screen Space Ambient Occlusion (SSAO).
 * Takes G-buffer data as input and outputs SSAO-modified final image.
 */
class SSAOShader : public Shader {
public:
    struct SSAOParams {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 samples[64];   // Sample kernel
        glm::vec2 screenSize;
        float radius;
        float bias;
        float padding[2];        // Align to 16 bytes
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
    size_t getUniformDataSize() const override { return sizeof(SSAOParams); }

    // SSAO specific methods
    void updateSSAOParams(const Camera& camera, float aspectRatio, VkExtent2D screenSize);
    void setGBufferTextures(VulkanRenderer* renderer, VkImageView positionView, VkImageView normalView, VkImageView albedoView, VkSampler sampler);

protected:
    void createDescriptorSetLayout(VkDevice device) override;
    void createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent) override;
    void createDescriptorSets(VulkanRenderer* renderer) override;

private:
    // SSAO-specific resources
    VkImage noiseImage = VK_NULL_HANDLE;
    VkDeviceMemory noiseImageMemory = VK_NULL_HANDLE;
    VkImageView noiseImageView = VK_NULL_HANDLE;
    VkSampler noiseSampler = VK_NULL_HANDLE;

    // G-buffer texture references (not owned by this shader)
    VkImageView gBufferPositionView = VK_NULL_HANDLE;
    VkImageView gBufferNormalView = VK_NULL_HANDLE;
    VkImageView gBufferAlbedoView = VK_NULL_HANDLE;
    VkSampler gBufferSampler = VK_NULL_HANDLE;

    // Helper methods
    void createNoiseTexture(VulkanRenderer* renderer);
    void generateSSAOKernel(SSAOParams& params);
    void cleanupNoiseTexture(VkDevice device);
};