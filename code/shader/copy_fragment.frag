#version 450

layout(location = 0) in vec2 fragTexCoord;

// G-buffer textures as input
layout(binding = 1) uniform sampler2D gPosition;
layout(binding = 2) uniform sampler2D gNormal;
layout(binding = 3) uniform sampler2D gAlbedo;

layout(location = 0) out vec4 outColor;

void main()
{
    // For now, just display the albedo (color) buffer
    // Later you can switch between different buffers for debugging
    vec3 albedo = texture(gAlbedo, fragTexCoord).rgb;

    // You can uncomment these lines to view other buffers:
    // vec3 position = texture(gPosition, fragTexCoord).rgb;
    // vec3 normal = texture(gNormal, fragTexCoord).rgb * 0.5 + 0.5; // Remap normals to [0,1]

    outColor = vec4(albedo, 1.0);
}