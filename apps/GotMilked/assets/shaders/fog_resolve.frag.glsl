#version 460 core

layout(location = 0) out vec4 FragColor;

in vec2 vUv;

uniform sampler3D uFroxelVolume;
uniform vec3 uFrustumCorners[8];
uniform int uGridDepth;
uniform float uIntensityScale;
uniform float uLightFactor;

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

void main() {
    int steps = max(uGridDepth, 1);
    float stepSize = 1.0 / float(steps);

    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < steps; ++i) {
        float slice = (float(i) + 0.5) * stepSize;
        vec4 fog = texture(uFroxelVolume, vec3(vUv, slice));
        float density = fog.a;
        vec3 color = fog.rgb * uIntensityScale;

        vec3 worldPos = ReconstructWorld(vUv, slice);
        float heightFade = clamp(exp(-max(worldPos.y, 0.0) * 0.02), 0.2, 1.0);

        float weight = density * stepSize;
        vec3 contribution = color * weight * transmittance * heightFade * uLightFactor;
        accumulated += contribution;
        transmittance *= exp(-density * stepSize * 1.2);
    }

    float alpha = 1.0 - transmittance;
    FragColor = vec4(accumulated, alpha);
}


