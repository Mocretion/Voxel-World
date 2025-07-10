#version 450

layout(location = 0) in vec2 fragTexCoord;

// G-buffer textures as input
layout(binding = 1) uniform sampler2D gPosition;
layout(binding = 2) uniform sampler2D gNormal;
layout(binding = 3) uniform sampler2D gAlbedo;

// Blurred SSAO texture
layout(binding = 4) uniform sampler2D blurredSSAO;

layout(location = 0) out vec4 outColor;

void main()
{
    // Sample the albedo (lit color)
    vec3 albedo = texture(gAlbedo, fragTexCoord).rgb;

    // Sample the blurred SSAO value
    float ssaoValue = texture(blurredSSAO, fragTexCoord).r;

    // Apply SSAO to the albedo
    vec3 finalColor = albedo * ssaoValue;

    outColor = vec4(finalColor, 1.0);
}