$input v_color, v_position

#include "bgfx_shader.sh"

void main()
{
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 normal = normalize(cross(dFdx(v_position), dFdy(v_position)));
    
    float lightIntensity = max(dot(normal, lightDir), 0.3);
    
    vec3 finalColor = v_color.rgb * lightIntensity;
    
    gl_FragColor = vec4(finalColor, 1.0);
}