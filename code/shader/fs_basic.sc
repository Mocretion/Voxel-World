$input v_color, v_normal

#include "bgfx_shader.sh"

void main()
{
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.5));
    vec3 normal = normalize(v_normal);
    
    float lightIntensity = max(dot(normal, lightDir), 0.3);
    
    vec3 finalColor = v_color.rgb * lightIntensity;
    
    gl_FragColor = vec4(finalColor, 1.0);
}