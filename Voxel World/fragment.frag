#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;  // NEW: Normal from vertex shader
layout(location = 2) in vec3 fragPos;     // NEW: World position

layout(location = 0) out vec4 outColor;

void main()
{
    // For now, just use the original color
    // Later you'll use fragNormal and fragPos for SSAO calculations

    // Optional: Add simple directional lighting to test normals
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));  // Light direction
    vec3 normal = normalize(fragNormal);              // Normalize the normal
    float light = max(dot(normal, lightDir), 0.3);   // Dot product + ambient

    vec3 litColor = fragColor * light;
    outColor = vec4(litColor, 1.0);
}