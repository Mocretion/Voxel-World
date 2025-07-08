#include "Shader.h"
#include "VulkanRenderer.h"
#include "ResourceManager.h"
#include <stdexcept>

VkShaderModule Shader::createShaderModule(VkDevice device, const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

void Shader::createUniformBuffers(VulkanRenderer* renderer, size_t bufferSize, size_t frameCount)
{
    uniformBuffers.resize(frameCount);
    uniformBuffersMemory.resize(frameCount);
    uniformBuffersMapped.resize(frameCount);

    for (size_t i = 0; i < frameCount; i++)
    {
        renderer->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(renderer->getDevice(), uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void Shader::cleanupUniformBuffers(VkDevice device)
{
    for (size_t i = 0; i < uniformBuffers.size(); i++)
    {
        if (uniformBuffers[i] != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }
    }
    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();
}

std::vector<char> Shader::loadShaderFile(const std::string& filename)
{
    return ResourceManager::readFile(ResourceManager::getShaderPath(filename));
}