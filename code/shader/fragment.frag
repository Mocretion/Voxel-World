#version 450

// Inputs from vertex shader
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec3 fragViewPos;

// G-buffer outputs (Multiple Render Targets)
layout(location = 0) out vec4 gPosition;  // World position + depth
layout(location = 1) out vec4 gNormal;    // World normal + unused
layout(location = 2) out vec4 gAlbedo;    // Base color + unused

void main()
{
    // Output 1: Position buffer
    // Store world position in RGB, and view-space depth in A for SSAO
    gPosition = vec4(fragWorldPos, fragViewPos.z);

    // Output 2: Normal buffer  
    // Store world-space normal in RGB, leave A unused for now
    gNormal = vec4(normalize(fragNormal), 1.0);

    // Output 3: Albedo/Color buffer
    // Apply simple directional lighting to the base color
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));  // Light direction
    vec3 normal = normalize(fragNormal);              // World normal
    float lightIntensity = max(dot(normal, lightDir), 0.3);  // Diffuse + ambient

    vec3 litColor = fragColor * lightIntensity;
    gAlbedo = vec4(litColor, 1.0);
}