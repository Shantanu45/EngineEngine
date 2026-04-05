#version 450 core
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;    
    float shininess;
} material; 

layout(binding = 1) uniform Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
} light;

layout(location = 0) in vec3 FragPos;  
layout(location = 1) in vec3 Normal;  
  
layout(binding = 2) uniform View {
    vec3 viewPos;
} view;

void main()
{
    // ambient
    vec3 ambient = light.ambient * material.ambient;
  	
    // diffuse 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);
    
    // specular
    vec3 viewDir = normalize(view.viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);  
        
    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
} 