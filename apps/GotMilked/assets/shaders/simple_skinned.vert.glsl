#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aPaintWeights; // Reserved to match static layout
layout(location = 4) in uvec4 aBoneIndices;
layout(location = 5) in vec4 aBoneWeights;

layout(std140, binding = 0) uniform SkinningPalette {
    mat4 uBones[128];
};

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormal;
out vec3 vFragPos;
out vec2 vUV;
out vec4 vPaintWeights;

mat4 ComputeSkinMatrix() {
    mat4 skinMat = mat4(0.0);
    skinMat += uBones[aBoneIndices.x] * aBoneWeights.x;
    skinMat += uBones[aBoneIndices.y] * aBoneWeights.y;
    skinMat += uBones[aBoneIndices.z] * aBoneWeights.z;
    skinMat += uBones[aBoneIndices.w] * aBoneWeights.w;
    return skinMat;
}

void main() {
    mat4 skinMatrix = uModel * ComputeSkinMatrix();

    vec4 worldPos = skinMatrix * vec4(aPos, 1.0);
    vFragPos = worldPos.xyz;

    mat3 normalMat = mat3(skinMatrix);
    vNormal = normalize(normalMat * aNormal);

    vUV = aUV;
    vPaintWeights = aPaintWeights;

    gl_Position = uProj * uView * worldPos;
}

