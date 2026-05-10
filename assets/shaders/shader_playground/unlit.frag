#version 450

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 tangent;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform PlaygroundFrame {
    vec2 iResolution;
    float iTime;
    float iDeltaTime;
    mat4 view;
    mat4 proj;
    vec4 iMouse;
} frame;

layout(set = 1, binding = 0) uniform sampler2D tex0;

layout(set = 1, binding = 1) uniform Params {
    vec4 tint;
    vec4 controls;
} params;

void main()
{
    vec3 base = texture(tex0, uv).rgb;
    vec3 n = normal * 0.5 + 0.5;
    float stripe = 0.5 + 0.5 * sin((worldPos.y + frame.iTime * params.controls.x) * 8.0);
    vec3 color = mix(base, n, params.controls.y) * mix(0.7, 1.0, stripe) * params.tint.rgb;
    outColor = vec4(color, params.tint.a);
}
