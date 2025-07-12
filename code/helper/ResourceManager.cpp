#include "ResourceManager.h"
#include <fstream>
#include <stdexcept>
#include <filesystem>

// Define the base path for shaders
const std::string ResourceManager::SHADER_PATH = "shaders/";

std::vector<char> ResourceManager::readFile(const std::string& filename)
{
    // Open file at the end to immediately get the file size
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    // Get file size and create buffer
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    // Go to beginning and read the entire file
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

bool ResourceManager::fileExists(const std::string& filename)
{
    return std::filesystem::exists(filename);
}

std::string ResourceManager::getShaderPath(const std::string& shaderName)
{
    return SHADER_PATH + shaderName;
}