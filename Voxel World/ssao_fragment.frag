// ============================================================================
// BALANCED ssao_fragment.frag - Fixed distance and face balance
// ============================================================================
#version 450

layout(location = 0) in vec2 fragTexCoord;

// G-buffer inputs
layout(binding = 1) uniform sampler2D gPosition;
layout(binding = 2) uniform sampler2D gNormal;
layout(binding = 3) uniform sampler2D gAlbedo;

// SSAO parameters
layout(binding = 0) uniform SSAOParams {
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

const int KERNEL_SIZE = 32; // Good balance of quality and performance
const float PI = 3.14159265359;

void main()
{
    // Sample G-buffer
    vec4 positionData = texture(gPosition, fragTexCoord);
    vec3 worldPos = positionData.xyz;
    float viewDepth = positionData.w;
    
    vec3 worldNormal = normalize(texture(gNormal, fragTexCoord).xyz);
    vec3 albedo = texture(gAlbedo, fragTexCoord).rgb;
    
    // Skip SSAO for background pixels
    if (length(worldNormal) < 0.1) {
        outColor = vec4(albedo, 1.0);
        return;
    }
    
    // Transform position and normal to view space
    vec4 viewPos4 = ssaoParams.view * vec4(worldPos, 1.0);
    vec3 viewPos = viewPos4.xyz;
    vec3 viewNormal = normalize(mat3(ssaoParams.view) * worldNormal);
    
    // EXTENDED DISTANCE FADE - much longer range
    float distanceFromCamera = length(viewPos);
    float distanceFade = 1.0 - smoothstep(50.0, 150.0, distanceFromCamera); // Extended from 20-50 to 50-150
    
    // If too far away, skip SSAO entirely
    if (distanceFade < 0.01) {
        outColor = vec4(albedo, 1.0);
        return;
    }
    
    // Get noise vector with temporal stability
    vec2 noiseCoord = (worldPos.xz * 0.05) + (fragTexCoord * ssaoParams.screenSize / 64.0);
    vec3 randomVec = normalize(texture(noiseTexture, noiseCoord).xyz);
    
    // Create TBN matrix
    vec3 tangent = normalize(randomVec - viewNormal * dot(randomVec, viewNormal));
    vec3 bitangent = cross(viewNormal, tangent);
    mat3 TBN = mat3(tangent, bitangent, viewNormal);
    
    float occlusion = 0.0;
    int validSamples = 0;
    
    // BALANCED adaptive radius - scale with distance but keep reasonable
    float baseRadius = ssaoParams.radius;
    float adaptiveRadius = baseRadius * (0.5 + 0.5 * (1.0 - smoothstep(10.0, 80.0, distanceFromCamera)));
    adaptiveRadius = max(adaptiveRadius, baseRadius * 0.4); // Minimum radius
    
    // BALANCED FACE TREATMENT - equal treatment for side faces
    float xNormalStrength = abs(worldNormal.x);
    float yNormalStrength = abs(worldNormal.y);
    float zNormalStrength = abs(worldNormal.z);
    
    // No radius modification based on face direction - treat all sides equally
    // Only slight adjustment for top faces if needed
    
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        // Get sample position in view space
        vec3 sampleOffset = TBN * ssaoParams.samples[i].xyz;
        vec3 samplePos = viewPos + sampleOffset * adaptiveRadius;
        
        // Project sample position to screen space
        vec4 offset = vec4(samplePos, 1.0);
        offset = ssaoParams.projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        
        // Skip samples outside screen bounds
        if (offset.x < 0.0 || offset.x > 1.0 || offset.y < 0.0 || offset.y > 1.0) {
            continue;
        }
        
        // Sample the G-buffer at this screen position
        vec4 sampleData = texture(gPosition, offset.xy);
        vec3 sampleWorldPos = sampleData.xyz;
        
        // Skip if we sampled background
        if (length(sampleWorldPos) < 0.1) {
            continue;
        }
        
        // Transform sampled world position to view space
        vec4 sampleViewPos4 = ssaoParams.view * vec4(sampleWorldPos, 1.0);
        float sampleDepth = sampleViewPos4.z;
        
        // Improved occlusion test
        float depthDifference = sampleDepth - samplePos.z;
        
        // Range check - prevent distant occlusion
        float maxRange = adaptiveRadius * 2.0;
        float rangeCheck = 1.0 - smoothstep(0.0, maxRange, abs(viewPos.z - sampleDepth));
        
        // Check for occlusion
        if (depthDifference > ssaoParams.bias && depthDifference < maxRange) {
            occlusion += rangeCheck;
        }
        
        validSamples++;
    }
    
    // Normalize occlusion
    if (validSamples > 0) {
        occlusion = occlusion / float(validSamples);
    }
    
    // Invert occlusion
    occlusion = 1.0 - occlusion;
    
    // BALANCED FACE-BASED SSAO STRENGTH
    float ssaoStrength = 1.0;
    
       
    // Apply mild contrast
    occlusion = pow(occlusion, 1.1);
    occlusion = clamp(occlusion, 0.4, 1.0); // Prevent too dark areas
    
    // Mix SSAO effect based on strength
    occlusion = mix(1.0, occlusion, distanceFade);
    
    // Apply SSAO to albedo
    vec3 finalColor = albedo * occlusion;
    
    outColor = vec4(finalColor, 1.0);
}