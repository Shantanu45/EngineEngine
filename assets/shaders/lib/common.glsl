struct CameraData {
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float _pad;
};

struct ObjectData {
    mat4 model;
    mat4 normalMatrix; // transpose(inverse(model)), precomputed on CPU
};