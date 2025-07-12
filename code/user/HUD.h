#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "Vertex.h"

class HUD {
public:
    HUD();

    // Update HUD with current camera direction
    void updateDirection(const glm::vec3& cameraFront);

    // Get vertices for rendering the HUD text
    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<uint16_t>& getIndices() const { return indices; }

    // Get the current direction string
    const std::string& getDirectionText() const { return directionText; }

private:
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    std::string directionText;

    // Helper methods
    std::string getDirectionString(const glm::vec3& front);
    void generateTextQuads();
    void addCharQuad(float x, float y, float width, float height, glm::vec3 color);
};