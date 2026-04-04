#version 450

layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform Colors {
    vec3 object_color;
    vec3 light_color;
} color;

void main()
{
    FragColor = vec4(color.light_color * color.object_color, 1.0);
}