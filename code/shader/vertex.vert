#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform ChunkPushConstants
{
    int chunkX;
    int chunkY;
    int chunkZ;
    int padding;
} chunkConstants;

layout(location = 0) in uint inPosition;
layout(location = 1) in uint inColor;

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;   // World-space normal
layout(location = 2) out vec3 fragWorldPos; // World-space position
layout(location = 3) out vec3 fragViewPos;  // View-space position (for SSAO depth)

vec3 unpackPosition(uint packedPos)
{
    uint x = (packedPos >> 0) & 0x7Fu;
    uint y = (packedPos >> 7) & 0x7Fu;
    uint z = (packedPos >> 14) & 0x7Fu;

    return vec3(x, y, z) + vec3(
        float(chunkConstants.chunkX * 64),
        float(chunkConstants.chunkY * 64),
        float(chunkConstants.chunkZ * 64)
    );
}

vec3 unpackNormals(uint packedNormals)
{
    vec3 normals[6] = {
    vec3(1, 0, 0),  // Right face (+X)
    vec3(-1, 0, 0), // Left face (-X)
    vec3(0, 1, 0),  // Top face (+Y)
    vec3(0, -1, 0), // Bottom face (-Y)
    vec3(0, 0, 1),  // Front face (+Z)
    vec3(0, 0, -1)  // Back face (-Z)
    };

    uint normalsId = (packedNormals >> 21) & 0x07u;

	return normals[normalsId];
}

vec3 unpackColors(uint packedColors)
{
    uint x = (packedColors >> 0) & 0xFFu;
    uint y = (packedColors >> 8) & 0xFFu;
    uint z = (packedColors >> 16) & 0xFFu;

    return vec3(x / 255.0, y / 255.0, z / 255.0);
}

void main()
{
    vec3 inPositionUnpacked = unpackPosition(inPosition);
    vec3 inNormalsUnpacked = unpackNormals(inPosition);
	vec3 inColorUnpacked = unpackColors(inColor);

    // Transform vertex position to world space
    vec4 worldPos = ubo.model * vec4(inPositionUnpacked, 1.0);

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
    
    fragNormal = normalize(mat3(ubo.model) * inNormalsUnpacked);

    // Pass color unchanged
    fragColor = inColorUnpacked;
}