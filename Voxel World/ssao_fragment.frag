#version 450

layout(location = 0) in vec2 fragTexCoord;

// G-buffer inputs
layout(binding = 1) uniform sampler2D gPosition;
layout(binding = 2) uniform sampler2D gNormal;
layout(binding = 3) uniform sampler2D gAlbedo;

// SSAO parameters
layout(binding = 0) uniform SSAOParams
{
    mat4 projection;
    mat4 view;
    vec4 samples[64];        // Sample kernel (64 samples)
    vec2 screenSize;         // Screen dimensions
    float radius;            // Sample radius
    float bias;              // Depth bias to prevent self-occlusion
} ssaoParams;

// Noise texture for randomizing samples
layout(binding = 4) uniform sampler2D noiseTexture;

layout(location = 0) out vec4 outColor;

// Constants
const int KERNEL_SIZE = 64;
const float PI = 3.14159265359;

void main()
{
    // Sample G-buffer
    vec3 worldPos = texture(gPosition, fragTexCoord).xyz;
    vec3 worldNormal = normalize(texture(gNormal, fragTexCoord).xyz);
    vec3 albedo = texture(gAlbedo, fragTexCoord).rgb;

    // Skip SSAO for background pixels (no geometry)
    if (length(worldNormal) < 0.1)
    {
        outColor = vec4(albedo, 1.0);
        return;
    }

    // Transform position and normal to view space
    vec4 viewPos4 = ssaoParams.view * vec4(worldPos, 1.0);
    vec3 viewPos = viewPos4.xyz;
    vec3 viewNormal = normalize(mat3(ssaoParams.view) * worldNormal);

    // Get noise vector for this pixel (tiled across screen)
    vec2 noiseScale = ssaoParams.screenSize / 4.0; // 4x4 noise texture
    vec3 randomVec = normalize(texture(noiseTexture, fragTexCoord * noiseScale).xyz);

    // Create TBN matrix to orient samples around surface normal
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);

    float occlusion = 0.0;

    // Sample around the current pixel
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        // Get sample position in view space
        vec3 samplePos = TBN * ssaoParams.samples[i].xyz; // Orient sample
        samplePos = viewPos + samplePos * ssaoParams.radius; // Scale and translate

        // Project sample position to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = ssaoParams.projection * offset;    // To clip space
        offset.xyz /= offset.w;                     // Perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5;       // To [0,1] range

        // Sample depth at this screen position
        vec3 sampleWorldPos = texture(gPosition, offset.xy).xyz;

        // Skip if we sampled outside the screen or background
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0 ||
            length(sampleWorldPos) < 0.1)
        {
            continue;
        }

        // Transform sampled world position to view space for comparison
        vec4 sampleViewPos4 = ssaoParams.view * vec4(sampleWorldPos, 1.0);
        float sampleDepth = sampleViewPos4.z;

        // Range check to avoid distant occlusion
        float rangeCheck = smoothstep(0.0, 1.0, ssaoParams.radius / abs(viewPos.z - sampleDepth));

        // Check if sample is occluded (behind surface)
        float occluded = (sampleDepth >= samplePos.z + ssaoParams.bias) ? 1.0 : 0.0;
        occlusion += occluded * rangeCheck;
    }

    // Normalize and invert occlusion (1.0 = no occlusion, 0.0 = full occlusion)
    occlusion = 1.0 - (occlusion / KERNEL_SIZE);

    // Apply SSAO to albedo
    vec3 finalColor = albedo * occlusion;

    outColor = vec4(finalColor, 1.0);
}