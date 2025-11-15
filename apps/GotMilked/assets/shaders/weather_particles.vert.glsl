#version 460 core
#extension GL_ARB_shader_draw_parameters : enable

layout(location = 0) in vec3 aQuad;

struct ParticleGPU {
    vec4 posLife;
    vec4 velSize;
    vec4 color;
};

struct EmitterMeta {
    uint baseInstance;
    uint pad0;
    uint pad1;
    uint pad2;
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    ParticleGPU particles[];
};

layout(std430, binding = 1) readonly buffer EmitterBuffer {
    EmitterMeta metas[];
};

uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

out VS_OUT {
    vec4 color;
    float alpha;
} vs_out;

void main() {
    uint baseInstance = metas[gl_DrawID].baseInstance;
    uint index = baseInstance + gl_InstanceID;
    ParticleGPU particle = particles[index];

    float life = particle.posLife.w;
    vec3 position = particle.posLife.xyz;
    vec3 color = particle.color.rgb;
    float size = particle.velSize.w;

    vec3 offset = (uCameraRight * aQuad.x + uCameraUp * aQuad.y) * size;
    vec4 worldPos = vec4(position + offset, 1.0f);

    gl_Position = uProj * uView * worldPos;
    vs_out.color = vec4(color, 1.0f);
    vs_out.alpha = clamp(1.0f - life, 0.1f, 1.0f);
}


