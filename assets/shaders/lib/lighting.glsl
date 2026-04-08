#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

struct Material {
    float shininess;
}; 

struct PointLight {
    vec3  position;
    float constant;   // 16 bytes

    vec3  ambient;
    float linear;     // 16 bytes

    vec3  diffuse;
    float quadratic;  // 16 bytes

    vec3  specular;
    float _pad;       // 16 bytes
};

// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, Material material, sampler2D diffuse_tex,
    sampler2D specular_tex, vec2 texCoords, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // halfway direction (blinn phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);     // phong shading
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);        // blinn phong
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // combine results
    vec3 ambient = light.ambient * vec3(texture(diffuse_tex, texCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(diffuse_tex, texCoords));
    vec3 specular = light.specular * spec * vec3(texture(specular_tex, texCoords));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
  
    float constant;
    float linear;
    float quadratic;
  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;       
};

vec3 CalcSpotLight(SpotLight light, Material material, sampler2D diffuse_tex,
    sampler2D specular_tex, vec2 texCoords, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results
    vec3 ambient = light.ambient * vec3(texture(diffuse_tex, texCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(diffuse_tex, texCoords));
    vec3 specular = light.specular * spec * vec3(texture(specular_tex, texCoords));
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}

struct DirLight {
    vec3 direction;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

vec3 CalcDirLight(DirLight light, Material material, sampler2D diffuse_tex,
    sampler2D specular_tex, vec2 texCoords, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 ambient = light.ambient * vec3(texture(diffuse_tex, texCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(diffuse_tex, texCoords));
    vec3 specular = light.specular * spec * vec3(texture(specular_tex, texCoords));
    return (ambient + diffuse + specular);
}


#endif