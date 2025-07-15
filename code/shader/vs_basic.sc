$input a_position, a_color0
$output v_color, v_position

uniform mat4 u_model;
uniform mat4 u_view; 
uniform mat4 u_proj;

vec3 unpackPosition(uint packedPos)
{
    uint x = (packedPos >> 0u) & 0x7Fu;
    uint y = (packedPos >> 7u) & 0x7Fu;
    uint z = (packedPos >> 14u) & 0x7Fu;

    return vec3(float(x), float(y), float(z));
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
    uint positionData = uint(a_position.x);
    uint colorData = uint(a_color0.x);
    
    vec3 position = unpackPosition(positionData);
    vec3 color = unpackColor(colorData);
    
    vec4 worldPos = mul(u_model, vec4(position, 1.0));
    vec4 viewPos = mul(u_view, worldPos);
    gl_Position = mul(u_proj, viewPos);
    
    v_color = vec4(color, 1.0);
    v_position = worldPos.xyz;
}