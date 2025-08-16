$input a_position, a_color0, i_data0
$output v_color, v_normal
uniform vec4 u_chunkPos;

#include "bgfx_shader.sh"

vec3 unpackPosition(uint packedPos, vec4 offset)
{
    uint x = (packedPos >> 0u) & 0x7Fu;
    uint y = (packedPos >> 7u) & 0x7Fu;
    uint z = (packedPos >> 14u) & 0x7Fu;

    return vec3(float(x), float(y), float(z));
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

vec3 unpackColor(uint packedColor)
{
    uint r = (packedColor >> 0u) & 0xFFu;
    uint g = (packedColor >> 8u) & 0xFFu;
    uint b = (packedColor >> 16u) & 0xFFu;

    return vec3(float(r) / 255.0, float(g) / 255.0, float(b) / 255.0);
}

void main()
{
    uint positionData = uint(a_position);
    uint colorData = uint(a_color0);
    
    vec3 localPosition = unpackPosition(positionData);
    vec3 normals = unpackNormals(positionData);
    vec3 color = unpackColor(colorData);
    
    vec3 chunkOffset = i_data0.xyz * 64.0;
    vec3 worldPosition = localPosition + chunkOffset;
    gl_Position = mul(u_modelViewProj, vec4(worldPosition, 1.0));
   
    v_color = vec4(color, 1.0);
    v_normal = normalize(mat3(u_model[0]) * normals);
}