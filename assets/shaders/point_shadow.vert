#version 450

#include "lib/common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(set = 0, binding = 0) uniform FrameData { 
    CameraData camera;
    float time;
    float _pad1;
    float _pad2;
    float _pad3;
    mat4 lightSpaceMatrix; 
} frame;

layout(push_constant) uniform PC { 
    mat4 model; 
    mat4 normalMatrix; 
} pc;

layout(location = 0) out vec4 FragPos;

void main() {
    FragPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = FragPos; // will be overwritten by geometry shader
}