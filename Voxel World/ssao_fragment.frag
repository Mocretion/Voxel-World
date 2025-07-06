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
    vec4 samples[64];
    vec2 screenSize;
    float radius;
    float bias;
} ssaoParams;

// Noise texture
layout(binding = 4) uniform sampler2D noiseTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    // Sample G-buffer
    vec3 worldPos = texture(gPosition, fragTexCoord).xyz;
    vec3 worldNormal = normalize(texture(gNormal, fragTexCoord).xyz);
    vec3 albedo = texture(gAlbedo, fragTexCoord).rgb;

    // DEBUG: Let's see what we're getting from the G-buffer

    // Test 1: Show albedo (should look normal)
    // outColor = vec4(albedo, 1.0);
    // return;

    // Test 2: Show normals as colors (should look rainbow-like)
    //outColor = vec4(worldNormal * 0.5 + 0.5, 1.0);
    //return;

    // Test 3: Show positions as colors (should look weird but colorful)
    // outColor = vec4(abs(worldPos) * 0.01, 1.0);
    // return;

    // Test 4: Show noise texture (should look noisy)
    // vec2 noiseScale = ssaoParams.screenSize / 4.0;
    // vec3 noise = texture(noiseTexture, fragTexCoord * noiseScale).xyz;
    // outColor = vec4(noise * 0.5 + 0.5, 1.0);
    // return;

    // Test 5: Simple occlusion test (darker = more occluded)
    if (length(worldNormal) < 0.1)
    {
        outColor = vec4(albedo, 1.0);
        return;
    }

    // Transform to view space
    vec4 viewPos4 = ssaoParams.view * vec4(worldPos, 1.0);
    vec3 viewPos = viewPos4.xyz;
    vec3 viewNormal = normalize(mat3(ssaoParams.view) * worldNormal);

    // Get noise
    vec2 noiseScale = ssaoParams.screenSize / 4.0;
    vec3 randomVec = normalize(texture(noiseTexture, fragTexCoord * noiseScale).xyz);

    // Create TBN matrix
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);

    float occlusion = 0.0;
    int validSamples = 0;

    // Simplified SSAO with fewer samples for debugging
    for (int i = 0; i < 16; ++i) // Using only 16 samples instead of 64
    {
        // Get sample position
        vec3 samplePos = TBN * ssaoParams.samples[i].xyz;
        samplePos = viewPos + samplePos * ssaoParams.radius;

        // Project to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = ssaoParams.projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        // Check bounds
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0)
        {
            continue;
        }

        // Sample depth
        vec3 sampleWorldPos = texture(gPosition, offset.xy).xyz;
        if (length(sampleWorldPos) < 0.1)
        {
            continue;
        }

        vec4 sampleViewPos4 = ssaoParams.view * vec4(sampleWorldPos, 1.0);
        float sampleDepth = sampleViewPos4.z;

        // Simple occlusion test
        if (sampleDepth >= samplePos.z + ssaoParams.bias)
        {
            occlusion += 1.0;
        }

        validSamples++;
    }

    if (validSamples > 0)
    {
        occlusion = occlusion / float(validSamples);
    }

    // Invert and apply stronger effect for debugging
    occlusion = 1.0 - occlusion;
    occlusion = pow(occlusion, 2.0); // Make effect more visible

    // Debug: Show occlusion as grayscale
    // outColor = vec4(vec3(occlusion), 1.0);
    // return;

    // Apply SSAO to albedo
    vec3 finalColor = albedo * occlusion;

    outColor = vec4(finalColor, 1.0);
}