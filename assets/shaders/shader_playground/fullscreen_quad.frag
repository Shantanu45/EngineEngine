#version 450

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform PlaygroundUBO {
    vec2  iResolution;
    float iTime;
    float _pad;
} ubo;

float pattern(vec2 uv) {
  uv = uv * 2.0 - 1.0;
  float t = uv.x * uv.x;//pow(uv.x * uv.x, 0.3) + pow(uv.y * uv.y, 0.3) - 1.0;
  return step(0.0, t) * t * 10.0 ;//+ step(0.2, t);
}

void main() {
  // Normalized pixel coordinates (from 0 to 1)
  vec2 uv = gl_FragCoord.xy / ubo.iResolution.xy;
  uv = uv * 2.0 - 1.0;
  vec2 grid = vec2(5.0, 3.0);
  vec2 tiled = fract(uv * grid);
  
  outColor = vec4(step(uv.y*uv.y, uv.x), 0.0, 0.0, 1.0);
}
