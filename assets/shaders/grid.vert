#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;
layout(location = 3) in vec4 inTangent;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec3 camera_pos;
    float pad;
} pc;

layout(location = 0) out vec3 cameraPos;
layout(location = 1) out vec3 worldPos;  // fix: vec3 not vec4

void main()
{
    worldPos  = inPosition;              // pass raw world position, not clip space
    gl_Position = pc.mvp * vec4(inPosition, 1.0);  // clip space goes to gl_Position only
    cameraPos = pc.camera_pos;
}