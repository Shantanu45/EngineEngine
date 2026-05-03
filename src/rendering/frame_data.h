#pragma once
#include <glm/glm.hpp>

struct alignas(16) CameraData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 cameraPos;
    float     _pad;
};

struct alignas(16) FrameData_UBO {
    CameraData camera;
    float      time;
    float      _pad[3];
    glm::mat4  light_space_matrix;
};

struct alignas(16) ObjectData_UBO {
    glm::mat4 model;
    glm::mat4 normalMatrix;
};

struct alignas(16) PointShadow_UBO {
    glm::mat4 shadowMatrices[6];
    glm::vec4 lightPos;  // xyz = pos, w = farPlane
};
