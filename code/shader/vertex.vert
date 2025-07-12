#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;   // World-space normal
layout(location = 2) out vec3 fragWorldPos; // World-space position
layout(location = 3) out vec3 fragViewPos;  // View-space position (for SSAO depth)

void main()
{
    // Transform vertex position to world space
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);

    // Transform to view space (camera space) for SSAO
    vec4 viewPos = ubo.view * worldPos;

    // Final clip space position
    gl_Position = ubo.proj * viewPos;

    // Pass world position to fragment shader
    fragWorldPos = worldPos.xyz;

    // Pass view position to fragment shader (useful for SSAO)
    fragViewPos = viewPos.xyz;

    // Transform normal to world space
    // Use mat3(ubo.model) to ignore translation and extract rotation/scale
    fragNormal = normalize(mat3(ubo.model) * inNormal);

    // Pass color unchanged
    fragColor = inColor;
}