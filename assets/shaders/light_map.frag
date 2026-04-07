#version 450 core
#include "lib/common.glsl"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoords;

layout(location = 0) out vec4 FragColor;

// FrameUBO repeated here — same set/binding as vert, GPU deduplicates it
layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
} frame;

layout(set = 0, binding = 1) uniform MaterialUBO {
    float shininess;
} material;

layout(set = 0, binding = 2) uniform LightUBO {
    vec4 position;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
} light;

layout(set = 0, binding = 3) uniform sampler2D diffuse_tex;
layout(set = 0, binding = 4) uniform sampler2D specular_tex;

void main()
{
    vec3 ambient  = light.ambient.rgb * texture(diffuse_tex, TexCoords).rgb;

    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(light.position.rgb - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = light.diffuse.rgb * diff * texture(diffuse_tex, TexCoords).rgb;

    vec3 viewDir    = normalize(frame.camera.cameraPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular   = light.specular.rgb * spec * texture(specular_tex, TexCoords).rgb;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}