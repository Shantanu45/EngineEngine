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

float near = 0.1; 
float far  = 100.0; 
  
float LinearizeDepth(float depth) 
{
    float z = depth * 2.0 - 1.0; // back to NDC 
    return (2.0 * near * far) / (far + near - z * (far - near));	
}


#endif