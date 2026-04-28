#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D src_rt;
layout(binding = 1) uniform sampler2D ui_rt;

vec4 linearToSRGB(vec4 c) {
    bvec4 cutoff = lessThan(c, vec4(0.0031308));
    vec4 low  = c * 12.92;
    vec4 high = 1.055 * pow(c, vec4(1.0 / 2.4)) - 0.055;
    return mix(high, low, cutoff);
}

vec3 srgbToLinear(vec3 c) {
    bvec3 cutoff = lessThan(c, vec3(0.04045));
    vec3 low  = c / 12.92;
    vec3 high = pow((c + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, cutoff);
}

void main()
{
    vec4 ui    = texture(ui_rt, uv);       // sRGB, treat as linear for blend
    vec4 scene = texture(src_rt, uv);      // linear

    // 1. composite in linear space
    vec4 composite = ui + scene * (1.0 - ui.a);

    // 2. ACES filmic tone mapping (Narkowicz 2015 approximation)
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    vec3 rgb = composite.rgb;
    composite.rgb = clamp((rgb * (a * rgb + b)) / (rgb * (c * rgb + d) + e), 0.0, 1.0);

    // 3. gamma correct the final result
    color = linearToSRGB(composite);
}