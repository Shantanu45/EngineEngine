#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

layout(binding = 0) uniform sampler2D src_rt;
layout(binding = 1) uniform sampler2D ui_rt;

layout(std140, binding = 2) uniform BlitSettings {
    vec4 toneMapping; // x=exposure, y=tone mapper: 0 none, 1 Reinhard, 2 ACES
} settings;

vec4 linearToSRGB(vec4 c) {
    c = max(c, vec4(0.0));
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

vec3 toneMapACES(vec3 rgb) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((rgb * (a * rgb + b)) / (rgb * (c * rgb + d) + e), 0.0, 1.0);
}

vec3 toneMap(vec3 rgb) {
    int toneMapper = int(settings.toneMapping.y + 0.5);
    if (toneMapper == 1)
        return rgb / (rgb + vec3(1.0));
    if (toneMapper == 2)
        return toneMapACES(rgb);
    return clamp(rgb, 0.0, 1.0);
}

void main()
{
    vec4 ui    = texture(ui_rt, uv);       // SDR UI, composite after scene presentation transform
    vec4 scene = texture(src_rt, uv);      // linear

    float exposure = max(settings.toneMapping.x, 0.0);
    scene.rgb = toneMap(scene.rgb * exposure);
    scene = linearToSRGB(scene);

    color = ui + scene * (1.0 - ui.a);
    color.a = 1.0;
}
