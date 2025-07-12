#pragma once

#include <vector>
#include <string>

class ResourceManager {
public:
    // Read file into byte vector (for shaders, etc.)
    static std::vector<char> readFile(const std::string& filename);

    // Check if file exists
    static bool fileExists(const std::string& filename);

    // Get full path for shader files
    static std::string getShaderPath(const std::string& shaderName);

private:
    // Base paths for different resource types
    static const std::string SHADER_PATH;
};