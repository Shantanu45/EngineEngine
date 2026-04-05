#version 450 core
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform Material {   
    float shininess;
} material; 
  
layout(binding = 1) uniform Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
} light;

layout(binding = 2) uniform View {
    vec3 viewPos;
} view;

layout(binding = 3) uniform sampler2D diffuse_tex;
layout(binding = 4) uniform sampler2D specular_tex; 

layout(location = 0) in vec3 FragPos;  
layout(location = 1) in vec3 Normal;  
layout(location = 2) in vec2 TexCoords;  

void main()
{
    // ambient
    vec3 ambient = light.ambient * texture(diffuse_tex, TexCoords).rgb;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texture(diffuse_tex, TexCoords).rgb;  
    
    // specular
    vec3 viewDir = normalize(view.viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * texture(specular_tex, TexCoords).rgb;  
        
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
} 