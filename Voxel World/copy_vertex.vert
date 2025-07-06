#version 450

// Full-screen triangle vertices (no vertex buffer needed)
vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),  // Bottom-left
    vec2(3.0, -1.0),  // Bottom-right (extended)
    vec2(-1.0, 3.0)   // Top-left (extended)
    );

vec2 texCoords[3] = vec2[](
    vec2(0.0, 0.0),    // Bottom-left
    vec2(2.0, 0.0),    // Bottom-right (extended)
    vec2(0.0, 2.0)     // Top-left (extended)
    );

layout(location = 0) out vec2 fragTexCoord;

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragTexCoord = texCoords[gl_VertexIndex];
}