#include "SSAOShader.h"
#include "VulkanRenderer.h"
#include <stdexcept>
#include <array>
#include <random>
#include <iostream>

void SSAOShader::initialize(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent)
{
    std::cout << "SSAOShader: Creating noise texture..." << std::endl;
    createNoiseTexture(renderer);

    std::cout << "SSAOShader: Creating descriptor set layout..." << std::endl;
    createDescriptorSetLayout(renderer->getDevice());

    std::cout << "SSAOShader: Creating pipeline..." << std::endl;
    createPipeline(renderer->getDevice(), renderPass, extent);

    std::cout << "SSAOShader: Creating uniform buffers..." << std::endl;
    createUniformBuffers(renderer, getUniformDataSize(), renderer->getMaxFramesInFlight());

    std::cout << "SSAOShader: Initialization complete!" << std::endl;
}

void SSAOShader::cleanup(VkDevice device)
{
    cleanupNoiseTexture(device);
    cleanupUniformBuffers(device);

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
}

void SSAOShader::recreate(VulkanRenderer* renderer, VkRenderPass renderPass, VkExtent2D extent)
{
    VkDevice device = renderer->getDevice();

    // Cleanup old pipeline
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    // Recreate noise texture with new dimensions
    cleanupNoiseTexture(device);
    createNoiseTexture(renderer);

    // Recreate pipeline
    createPipeline(device, renderPass, extent);

    // Update descriptor sets with new noise texture
    createDescriptorSets(renderer);
}

void SSAOShader::updateUniforms(uint32_t frameIndex, const void* data)
{
    if (frameIndex < uniformBuffersMapped.size() && uniformBuffersMapped[frameIndex])
    {
        memcpy(uniformBuffersMapped[frameIndex], data, getUniformDataSize());
    }
}

void SSAOShader::recordCommands(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
    static int frameCount = 0;
    if (frameCount++ % 60 == 0)
    {  // Print every 60 frames to avoid spam
        std::cout << "SSAOShader: Recording commands for frame " << frameIndex << std::endl;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    if (frameIndex < descriptorSets.size())
    {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout, 0, 1, &descriptorSets[frameIndex], 0, nullptr);
    }

    // Draw fullscreen triangle
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void SSAOShader::updateSSAOParams(const Camera& camera, float aspectRatio, VkExtent2D screenSize)
{
    SSAOParams params{};

    params.view = camera.getViewMatrix();
    params.projection = glm::perspective(glm::radians(80.0f), aspectRatio, 0.1f, 1000.0f);
    params.projection[1][1] *= -1; // Flip Y for Vulkan

    params.screenSize = glm::vec2(screenSize.width, screenSize.height);
    params.radius = 1.2f;
    params.bias = 0.08f;

    generateSSAOKernel(params);

    // Update all frames
    for (size_t i = 0; i < uniformBuffersMapped.size(); i++)
    {
        updateUniforms(static_cast<uint32_t>(i), &params);
    }
}

void SSAOShader::setGBufferTextures(VulkanRenderer* renderer, VkImageView positionView, VkImageView normalView, VkImageView albedoView, VkSampler sampler)
{
    gBufferPositionView = positionView;
    gBufferNormalView = normalView;
    gBufferAlbedoView = albedoView;
    gBufferSampler = sampler;

    // Now create the descriptor sets with the G-buffer textures
    createDescriptorSets(renderer);
}

void SSAOShader::createDescriptorSetLayout(VkDevice device)
{
    std::array<VkDescriptorSetLayoutBinding, 5> bindings{};

    // SSAO parameters uniform buffer (binding 0)
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].pImmutableSamplers = nullptr;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    // G-buffer textures (bindings 1, 2, 3)
    for (int i = 1; i <= 3; i++)
    {
        bindings[i].binding = i;
        bindings[i].descriptorCount = 1;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].pImmutableSamplers = nullptr;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    // Noise texture (binding 4)
    bindings[4].binding = 4;
    bindings[4].descriptorCount = 1;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].pImmutableSamplers = nullptr;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SSAO descriptor set layout!");
    }
}

void SSAOShader::createPipeline(VkDevice device, VkRenderPass renderPass, VkExtent2D extent)
{
    auto vertShaderCode = loadShaderFile("ssao_vert.spv");
    auto fragShaderCode = loadShaderFile("ssao_only_frag.spv");  // CHANGED: Use new SSAO-only fragment shader

    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // No vertex input (fullscreen triangle)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // CHANGED: Single render target for SSAO output
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;  // CHANGED: Single attachment
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SSAO pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create SSAO graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void SSAOShader::createDescriptorSets(VulkanRenderer* renderer)
{
    std::vector<VkDescriptorSetLayout> layouts(renderer->getMaxFramesInFlight(), descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = renderer->getDescriptorPool();
    allocInfo.descriptorSetCount = static_cast<uint32_t>(renderer->getMaxFramesInFlight());
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(renderer->getMaxFramesInFlight());
    if (vkAllocateDescriptorSets(renderer->getDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate SSAO descriptor sets!");
    }

    for (size_t i = 0; i < renderer->getMaxFramesInFlight(); i++)
    {
        std::array<VkWriteDescriptorSet, 5> descriptorWrites{};

        // SSAO uniform buffer (binding 0)
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = getUniformDataSize();

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        // G-buffer textures
        VkDescriptorImageInfo imageInfos[4];
        imageInfos[0] = { gBufferSampler, gBufferPositionView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[1] = { gBufferSampler, gBufferNormalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[2] = { gBufferSampler, gBufferAlbedoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[3] = { noiseSampler, noiseImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        for (int j = 1; j <= 4; j++)
        {
            descriptorWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[j].dstSet = descriptorSets[i];
            descriptorWrites[j].dstBinding = j;
            descriptorWrites[j].dstArrayElement = 0;
            descriptorWrites[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[j].descriptorCount = 1;
            descriptorWrites[j].pImageInfo = &imageInfos[j - 1];
        }

        vkUpdateDescriptorSets(renderer->getDevice(), static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(), 0, nullptr);
    }
}

void SSAOShader::createNoiseTexture(VulkanRenderer* renderer)
{
    std::cout << "SSAOShader: Starting noise texture creation..." << std::endl;

    const int noiseSize = 64;
    std::vector<glm::vec3> noiseData(noiseSize * noiseSize);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (int i = 0; i < noiseSize * noiseSize; i++)
    {
        glm::vec3 noise(dis(gen), dis(gen), 0.0f);
        noiseData[i] = glm::normalize(noise);
    }

    VkDeviceSize imageSize = noiseSize * noiseSize * 3 * sizeof(float);
    std::cout << "SSAOShader: Generated noise data, size: " << imageSize << " bytes" << std::endl;

    try
    {
        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        std::cout << "SSAOShader: Creating staging buffer..." << std::endl;
        renderer->createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingBufferMemory);

        std::cout << "SSAOShader: Mapping and copying data to staging buffer..." << std::endl;
        void* data;
        vkMapMemory(renderer->getDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, noiseData.data(), static_cast<size_t>(imageSize));
        vkUnmapMemory(renderer->getDevice(), stagingBufferMemory);

        // Create noise image
        std::cout << "SSAOShader: Creating noise image..." << std::endl;
        renderer->createImage(noiseSize, noiseSize, VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, noiseImage, noiseImageMemory);

        std::cout << "SSAOShader: Transitioning image layouts and copying data..." << std::endl;
        renderer->transitionImageLayout(noiseImage, VK_FORMAT_R32G32B32_SFLOAT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        renderer->copyBufferToImage(stagingBuffer, noiseImage, noiseSize, noiseSize);

        renderer->transitionImageLayout(noiseImage, VK_FORMAT_R32G32B32_SFLOAT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Create image view and sampler
        std::cout << "SSAOShader: Creating image view..." << std::endl;
        noiseImageView = renderer->createImageView(noiseImage, VK_FORMAT_R32G32B32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

        std::cout << "SSAOShader: Creating sampler..." << std::endl;
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 8.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(renderer->getDevice(), &samplerInfo, nullptr, &noiseSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create noise sampler!");
        }

        // Cleanup staging buffer
        std::cout << "SSAOShader: Cleaning up staging buffer..." << std::endl;
        vkDestroyBuffer(renderer->getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(renderer->getDevice(), stagingBufferMemory, nullptr);

        std::cout << "SSAOShader: Noise texture created successfully!" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "SSAOShader: Error creating noise texture: " << e.what() << std::endl;
        throw;
    }
}

void SSAOShader::generateSSAOKernel(SSAOParams& params)
{
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    for (int i = 0; i < 64; ++i)
    {
        glm::vec3 sample(
            dis(gen) * 2.0f - 1.0f,
            dis(gen) * 2.0f - 1.0f,
            dis(gen)
        );

        sample = glm::normalize(sample);

        float scale = float(i) / 64.0f;
        if (i < 16) scale = 0.1f + scale * 0.3f;
        else if (i < 48) scale = 0.4f + scale * 0.4f;
        else scale = 0.8f + scale * 0.2f;

        sample *= scale;
        params.samples[i] = glm::vec4(sample, 0.0f);
    }
}

void SSAOShader::cleanupNoiseTexture(VkDevice device)
{
    if (noiseSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, noiseSampler, nullptr);
        noiseSampler = VK_NULL_HANDLE;
    }
    if (noiseImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, noiseImageView, nullptr);
        noiseImageView = VK_NULL_HANDLE;
    }
    if (noiseImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, noiseImage, nullptr);
        noiseImage = VK_NULL_HANDLE;
    }
    if (noiseImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, noiseImageMemory, nullptr);
        noiseImageMemory = VK_NULL_HANDLE;
    }
}