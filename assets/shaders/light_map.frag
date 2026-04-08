#version 450 core
#include "lib/common.glsl"
#include "lib/lighting.glsl"

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 TexCoords;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform FrameUBO {
    CameraData camera;
    float time;
} frame;

layout(set = 0, binding = 1) uniform MaterialUBO {
    Material material;
} mat;

layout(set = 0, binding = 2) uniform LightBuffer {
    uint  lightCount;
    float _pad;
    float _pad1;
    float _pad2;
    Light lights[MAX_LIGHTS];
} lightData;

layout(set = 0, binding = 3) uniform sampler2D diffuse_tex;
layout(set = 0, binding = 4) uniform sampler2D specular_tex;

void main()
{
    vec3 normal  = normalize(Normal);
    vec3 viewDir = normalize(frame.camera.cameraPos - FragPos);

    // Global ambient — one cheap sample, not multiplied per light
    vec3 color = vec3(0.05) * vec3(texture(diffuse_tex, TexCoords));

    for (uint i = 0u; i < lightData.lightCount; i++) {
        color += CalcLight(
            lightData.lights[i], mat.material,
            diffuse_tex, specular_tex,
            TexCoords, normal, FragPos, viewDir);
    }

    FragColor = vec4(color, 1.0);
}