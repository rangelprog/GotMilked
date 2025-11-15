#version 460 core

layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

struct FogVolume {
    vec4 positionRadius;
    vec4 densityFalloffMaxDistanceEnabled;
    vec4 colorNoiseScale;
    vec4 noiseSpeedPad;
};

layout(std430, binding = 0) readonly buffer FogVolumes {
    FogVolume volumes[];
};

layout(binding = 0, rgba16f) uniform writeonly image3D uFroxelImage;
layout(binding = 1) uniform sampler3D uHistoryTexture;

uniform ivec3 uGridSize;
uniform int uVolumeCount;
uniform float uNearPlane;
uniform float uFarPlane;
uniform float uTemporalAlpha;
uniform float uTime;
uniform bool uHistoryValid;
uniform mat4 uPrevViewProj;
uniform vec3 uFrustumCorners[8];

vec3 SampleCorner(vec2 uv, bool farPlane) {
    vec3 bottom = mix(uFrustumCorners[farPlane ? 4 : 0],
                      uFrustumCorners[farPlane ? 5 : 1], uv.x);
    vec3 top = mix(uFrustumCorners[farPlane ? 7 : 3],
                   uFrustumCorners[farPlane ? 6 : 2], uv.x);
    return mix(bottom, top, uv.y);
}

vec3 ReconstructWorld(vec2 uv, float depthLerp) {
    vec3 nearCorner = SampleCorner(uv, false);
    vec3 farCorner = SampleCorner(uv, true);
    return mix(nearCorner, farCorner, depthLerp);
}

float ComputeNoise(vec3 worldPos, float scale, float speed) {
    const vec3 offset = vec3(12.9898, 78.233, 37.719);
    float n = dot(worldPos * scale + speed * uTime, offset);
    return fract(sin(n) * 43758.5453);
}

void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(coord, uGridSize))) {
        return;
    }

    vec3 uvw = (vec3(coord) + 0.5) / vec3(uGridSize);
    vec2 uv = uvw.xy;
    float depth = uvw.z;

    vec3 worldPos = ReconstructWorld(uv, depth);

    vec4 accum = vec4(0.0);
    for (int i = 0; i < uVolumeCount; ++i) {
        FogVolume volume = volumes[i];
        vec3 toPoint = worldPos - volume.positionRadius.xyz;
        float distanceToCenter = length(toPoint);
        float radius = max(volume.positionRadius.w, 0.01);
        float normalizedDistance = distanceToCenter / max(volume.densityFalloffMaxDistanceEnabled.z, radius);
        float density = volume.densityFalloffMaxDistanceEnabled.x;
        float falloff = exp(-normalizedDistance * 2.0);

        float heightFalloff = exp(-max(worldPos.y - volume.positionRadius.y, 0.0) *
                                  volume.densityFalloffMaxDistanceEnabled.y);

        float noise = mix(0.8, 1.2,
                          ComputeNoise(worldPos,
                                       volume.colorNoiseScale.w,
                                       volume.noiseSpeedPad.x));

        float influence = density * falloff * heightFalloff * noise;
        influence = clamp(influence, 0.0, 5.0);
        accum.rgb += volume.colorNoiseScale.rgb * influence;
        accum.a += influence * 0.5;
    }

    accum = clamp(accum, vec4(0.0), vec4(32.0));

    if (uHistoryValid) {
        vec4 history = texture(uHistoryTexture, uvw);
        accum = mix(history, accum, uTemporalAlpha);
    }

    imageStore(uFroxelImage, coord, accum);
}


