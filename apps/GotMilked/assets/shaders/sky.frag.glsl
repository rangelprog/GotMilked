#version 460 core

const float PI = 3.14159265;

in vec2 vUV;
out vec4 FragColor;

uniform vec3 uSunDirection;
uniform vec3 uGroundAlbedo;
uniform vec3 uCameraPos;
uniform float uTurbidity;
uniform float uExposure;
uniform float uAirDensity;

vec3 HosekEval(vec3 A, vec3 B, vec3 C, vec3 D, vec3 E, vec3 F, vec3 G, vec3 H, vec3 I,
               float cosTheta, float gamma, float cosGamma) {
    vec3 chi = (1.0 + cosGamma * cosGamma) / sqrt(1.0 + H);
    return (1.0 + A * exp(B / (cosTheta + 0.01))) *
           (C + D * exp(E * gamma) + F * cosGamma * cosGamma + G * chi + I * sqrt(cosTheta));
}

void main() {
    vec2 screenUV = vUV * 2.0 - 1.0;
    vec3 viewDir = normalize(vec3(screenUV, 1.0));

    float cosTheta = clamp(viewDir.y, 0.0, 1.0);
    float cosGamma = dot(viewDir, normalize(uSunDirection));
    float gammaAngle = acos(clamp(cosGamma, -1.0, 1.0));

    // Placeholder coefficients approximating Hosek; replace with real fit later.
    vec3 A = vec3(0.25f) * uTurbidity * 0.1;
    vec3 B = vec3(-0.1f);
    vec3 C = mix(vec3(0.1f, 0.2f, 0.5f), vec3(0.4f, 0.5f, 0.9f), cosTheta);
    vec3 D = vec3(0.0f);
    vec3 E = vec3(0.0f);
    vec3 F = vec3(0.0f);
    vec3 G = vec3(0.0f);
    vec3 H = vec3(0.0f);
    vec3 I = vec3(0.0f);

    vec3 radiance = HosekEval(A, B, C, D, E, F, G, H, I, cosTheta, gammaAngle, cosGamma);
    radiance *= clamp(uAirDensity, 0.25, 2.0);
    radiance = mix(radiance, uGroundAlbedo, 0.2f * (1.0f - cosTheta));

    vec3 sunGlow = vec3(1.0, 0.9, 0.7) * pow(max(cosGamma, 0.0f), 64.0);
    vec3 color = radiance + sunGlow;
    color = vec3(1.0) - exp(-color * uExposure);

    FragColor = vec4(color, 1.0);
}

