#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>

class Camera {
public:

    // Constructor
    Camera(glm::vec3 position = glm::vec3(0.0f, 30.0f, 0.0f));

    // Get the view matrix for rendering
    glm::mat4 getViewMatrix() const;

    // Handle input events
    void processKeyboard(const Uint8* keyState, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset);

    // Getters
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    glm::vec3 getUp() const { return up; }
    glm::vec3 getRight() const { return right; }

    // Camera settings - IMPROVED VALUES
    float movementSpeed = 15.0f;       // How fast we move (units per second)
    float mouseSensitivity = 0.1f;    // INCREASED from 0.1f - was too low!
    float pitch = 0.0f;                // Up/down rotation (in degrees)
    float yaw = -90.0f;                // Left/right rotation (in degrees, -90 = facing forward)

private:
    // Camera position and orientation
    glm::vec3 position;
    glm::vec3 front;     // Direction camera is facing
    glm::vec3 up;        // Up direction
    glm::vec3 right;     // Right direction
    glm::vec3 worldUp;   // World's up direction (usually 0, 1, 0)

    // Update camera vectors based on current yaw and pitch
    void updateCameraVectors();
};