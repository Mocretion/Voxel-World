#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;    // NEW: Normal input

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal; // NEW: Pass normal to fragment shader
layout(location = 2) out vec3 fragPos;    // NEW: World position for SSAO

void main()
{
    // Transform vertex position
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    // Pass data to fragment shader
    fragColor = inColor;
    fragPos = worldPos.xyz;

    // Transform normal to world space (important for lighting!)
    // mat3(ubo.model) extracts just the rotation/scale part, ignoring translation
    fragNormal = mat3(ubo.model) * inNormal;
}