#version 450

layout(location = 0) in vec2 fragTexCoord;

// Blur parameters
layout(binding = 0) uniform BlurParams
{
    vec2 blurDirection;      // (1,0) for horizontal, (0,1) for vertical
    float blurRadius;        // Blur kernel radius in pixels
    float depthThreshold;    // Depth difference threshold for edge detection
} blurParams;

// Input textures
layout(binding = 1) uniform sampler2D ssaoTexture;  // SSAO to blur
layout(binding = 2) uniform sampler2D depthTexture; // Depth for edge detection

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texelSize = 1.0 / textureSize(ssaoTexture, 0);

    // Center sample
    float centerDepth = texture(depthTexture, fragTexCoord).r;
    float result = texture(ssaoTexture, fragTexCoord).r;
    float totalWeight = 1.0;

    // Bilateral blur - sample along blur direction
    int kernelRadius = int(blurParams.blurRadius);

    for (int i = 1; i <= kernelRadius; i++)
    {
        vec2 offset = blurParams.blurDirection * texelSize * float(i);

        // Sample both sides of center
        for (int side = -1; side <= 1; side += 2)
        {
            vec2 sampleCoord = fragTexCoord + offset * float(side);

            // Skip if outside texture bounds
            if (sampleCoord.x < 0.0 || sampleCoord.x > 1.0 ||
                sampleCoord.y < 0.0 || sampleCoord.y > 1.0)
                continue;

            float sampleDepth = texture(depthTexture, sampleCoord).r;
            float sampleSSAO = texture(ssaoTexture, sampleCoord).r;

            // Bilateral weight based on depth difference (edge preservation)
            float depthDiff = abs(centerDepth - sampleDepth);
            float bilateralWeight = exp(-depthDiff / blurParams.depthThreshold);

            // Adaptive strength based on surface flatness
            float surfaceFlatness = 1.0 - smoothstep(0.0, blurParams.depthThreshold * 0.5, depthDiff);
            float adaptiveStrength = mix(0.5, 2.0, surfaceFlatness); // Stronger on flat surfaces

            // Gaussian weight based on distance
            float distance = float(i);
            float gaussianWeight = exp(-(distance * distance) / (2.0 * blurParams.blurRadius * blurParams.blurRadius));

            // Combined weight
            float weight = bilateralWeight * gaussianWeight * adaptiveStrength;

            result += sampleSSAO * weight;
            totalWeight += weight;
        }
    }

    // Normalize result
    result /= totalWeight;

    // Output blurred SSAO (keeping original color channels for debugging)
    vec3 originalColor = texture(ssaoTexture, fragTexCoord).rgb;
    outColor = vec4(result, result, result, 1.0);
}