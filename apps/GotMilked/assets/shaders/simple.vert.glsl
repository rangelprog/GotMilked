#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aPaintWeights;

layout(std430, binding = 4) readonly buffer InstanceModelBuffer {
    mat4 instanceModels[];
};

layout(std430, binding = 5) readonly buffer InstanceNormalBuffer {
    mat4 instanceNormals[];
};

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;
uniform bool uUseInstanceBuffers = false;

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vUV;
out vec4 vPaintWeights;

void main(){
    mat4 model = uUseInstanceBuffers ? instanceModels[gl_InstanceID] : uModel;
    mat3 normalMat = uUseInstanceBuffers ? mat3(instanceNormals[gl_InstanceID]) : uNormalMat;

    vec4 worldPos = model * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;
    vNormal  = normalize(normalMat * aNormal);
    vUV      = aUV;
    vPaintWeights = aPaintWeights;
    gl_Position = uProj * uView * worldPos;
}
