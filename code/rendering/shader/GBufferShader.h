#pragma once

#include "Shader.h"
#include "Vertex.h"

/**
 * Shader responsible for rendering 3D geometry to the G-buffer.
 * Outputs position, normal, and albedo data for deferred rendering.
 */
class GBufferShader : public Shader {
public:
    // Shader interface implementation
    void initialize(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) override;
    void cleanup(VkDevice device) override;
    void recreate(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent) override;

    void updateUniforms(uint32_t frameIndex, const void* data) override;
    void recordCommands(VkCommandBuffer commandBuffer, uint32_t frameIndex) override;

    // Query methods
    bool needsVertexInput() const override { return true; }
    bool hasUniformData() const override { return true; }
    size_t getUniformDataSize() const override { return sizeof(UniformBufferObject); }

    // G-buffer specific methods
    void bindVertexData(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void drawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount);

protected:
    void createDescriptorSetLayout(VkDevice device) override;
    void createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent) override;
    void createDescriptorSets(VulkanRenderer* renderer) override;

private:
    // Shader-specific state
    bool wireframeMode = false;

public:
    // Wireframe control
    void setWireframeMode(bool enabled) { wireframeMode = enabled; }
    bool getWireframeMode() const { return wireframeMode; }
};