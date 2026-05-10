#version 450
layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec3 cameraPos;  
layout(location = 1) in vec3 worldPos;  

void main()
{
    vec3 gridColor = vec3(0.5, 0.5, 0.5); 
    
    float fadeStart = 7.0;
    float fadeEnd   = 10.0;

    float dist = length(worldPos.xz - cameraPos.xz);
    float fade = 1.0 - smoothstep(fadeStart, fadeEnd, dist);
    
    FragColor = vec4(gridColor, fade); 
}