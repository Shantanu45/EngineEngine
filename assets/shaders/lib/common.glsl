// lib/common.glsl
#ifndef COMMON_GLSL
#define COMMON_GLSL

struct CameraData {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float _pad;
};

struct ObjectData {
    mat4 model;
    mat4 normalMatrix;
};

#endif