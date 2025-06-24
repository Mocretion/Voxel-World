#include "Camera.h"
#include <algorithm>

Camera::Camera(glm::vec3 position)
    : position(position), worldUp(glm::vec3(0.0f, 1.0f, 0.0f))
{
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    // Create view matrix using position, target (position + front), and up vector
    return glm::lookAt(position, position + front, up);
}

void Camera::processKeyboard(const Uint8* keyState, float deltaTime)
{
    // Calculate movement speed for this frame
    float velocity = movementSpeed * deltaTime;

    // WASD movement
    if (keyState[SDL_SCANCODE_W])
    {
        position += front * velocity;  // Move forward
    }
    if (keyState[SDL_SCANCODE_S])
    {
        position -= front * velocity;  // Move backward
    }
    if (keyState[SDL_SCANCODE_A])
    {
        position -= right * velocity;  // Move left
    }
    if (keyState[SDL_SCANCODE_D])
    {
        position += right * velocity;  // Move right
    }

    // Flying controls
    if (keyState[SDL_SCANCODE_SPACE])
    {
        position += worldUp * velocity;  // Move up
    }
    if (keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT])
    {
        position -= worldUp * velocity;  // Move down
    }

    // Optional: Speed boost
    if (keyState[SDL_SCANCODE_LCTRL] || keyState[SDL_SCANCODE_RCTRL])
    {
        // If holding Ctrl, move faster
        float boostVelocity = movementSpeed * 3.0f * deltaTime;

        if (keyState[SDL_SCANCODE_W])
        {
            position += front * boostVelocity;
        }
        if (keyState[SDL_SCANCODE_S])
        {
            position -= front * boostVelocity;
        }
        if (keyState[SDL_SCANCODE_A])
        {
            position -= right * boostVelocity;
        }
        if (keyState[SDL_SCANCODE_D])
        {
            position += right * boostVelocity;
        }
        if (keyState[SDL_SCANCODE_SPACE])
        {
            position += worldUp * boostVelocity;
        }
        if (keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT])
        {
            position -= worldUp * boostVelocity;
        }
    }
}

void Camera::processMouseMovement(float xOffset, float yOffset)
{
    // Apply mouse sensitivity
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    // Update yaw (left/right) and pitch (up/down)
    yaw += xOffset;
    pitch += yOffset;

    // Constrain pitch to prevent screen flipping
    // Clamp between -89 and +89 degrees
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    // Update camera vectors with new angles
    updateCameraVectors();
}

void Camera::updateCameraVectors()
{
    // Convert yaw and pitch from degrees to radians
    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);

    // Calculate new front vector
    glm::vec3 newFront;
    newFront.x = cos(yawRad) * cos(pitchRad);
    newFront.y = sin(pitchRad);
    newFront.z = sin(yawRad) * cos(pitchRad);

    front = glm::normalize(newFront);

    // Calculate right and up vectors
    // Right vector: cross product of front and world up
    right = glm::normalize(glm::cross(front, worldUp));

    // Up vector: cross product of right and front
    up = glm::normalize(glm::cross(right, front));
}