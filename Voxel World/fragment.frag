#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in float fragAO;
layout(location = 0) out vec4 outColor;

void main()
{
    // Apply ambient occlusion as a multiplier to darken occluded areas
    // AO ranges from 0.0 (fully occluded/dark) to 1.0 (no occlusion/bright)
    vec3 finalColor = fragColor * fragAO;

    // Optional: Add a minimum brightness to prevent completely black areas
    // This ensures some detail is still visible in heavily occluded corners
    float minBrightness = 0.3;
    finalColor = max(finalColor, fragColor * minBrightness);

    outColor = vec4(finalColor, 1.0);
}