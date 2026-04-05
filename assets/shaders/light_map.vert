#version 450 core

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 FragPos;
layout(location = 1) out vec3 Normal;
layout(location = 2) out vec2 TexCoords;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view_projection;
} pc;

void main()
{
    FragPos = vec3(pc.model * vec4(inPosition, 1.0));
    Normal = mat3(transpose(inverse(pc.model))) * inNormal;  
    TexCoords = inTexcoord;
    gl_Position = pc.view_projection * vec4(FragPos, 1.0);
}