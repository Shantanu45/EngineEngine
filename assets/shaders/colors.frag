#version 450

layout(location = 0) in vec3 FragPos;  
layout(location = 1) in vec3 Normal;  

layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform Colors {
    vec3 object_color;
    vec3 light_color;
    vec3 light_pos;
    vec3 view_pos;
} color;

void main()
{
   float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * color.light_color;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(color.light_pos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * color.light_color;

    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(color.view_pos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * color.light_color;  
            
    vec3 result = (ambient + diffuse + specular) * color.object_color;
    FragColor = vec4(result, 1.0);
}